/*
*
* centericq core routines
* $Id: centericq.cc,v 1.59 2001/12/10 14:00:42 konst Exp $
*
* Copyright (C) 2001 by Konstantin Klyagin <konst@konst.org.ua>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or (at
* your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
* USA
*
*/

#include "centericq.h"
#include "icqconf.h"
#include "icqhook.h"
#include "yahoohook.h"
#include "icqface.h"
#include "icqcontact.h"
#include "icqcontacts.h"
#include "icqmlist.h"
#include "icqgroups.h"
#include "accountmanager.h"

centericq::centericq() {
    timer_keypress = time(0)-50;
    timer_checkmail = timer_update = timer_resend = 0;
    regmode = false;
}

centericq::~centericq() {
}

void centericq::exec() {
    struct sigaction sact;

    memset(&sact, 0, sizeof(sact));
    sact.sa_handler = &handlesignal;
    sigaction(SIGINT, &sact, 0);
    sigaction(SIGCHLD, &sact, 0);
    sigaction(SIGALRM, &sact, 0);

    conf.initpairs();
    conf.load();
    face.init();

    if(regmode = !conf.getouridcount()) {
	char *p = getenv("LANG");

	if(p)
	if(((string) p).substr(0, 2) == "ru")
	    conf.setrussian(true);

	if(updateconf()) {
	    manager.exec();
	}

	regmode = false;
    }

    if(conf.getouridcount()) {
	conf.checkdir();
	conf.load();
	groups.load();
	clist.load();
	lst.load();
	conf.loadsounds();

	face.done();
	face.init();

	face.draw();
	checkparallel();
	inithooks();

	if(checkpasswords()) {
	    mainloop();
	}

	lst.save();
	clist.save();
	groups.save();
    }

    face.done();
    conf.save();
}

bool centericq::checkpasswords() {
    protocolname pname;
    icqconf::imaccount ia;
    bool r;

    r = regmode = true;

    for(pname = icq; pname != protocolname_size; (int) pname += 1) {
	ia = conf.getourid(pname);
	if(!ia.empty()) {
	    if(ia.password.empty()) {
		conf.setsavepwd(false);

		ia.password = face.inputstr("[" +
		    conf.getprotocolname(pname) + "] " +
		    _("password: "), "", '*');

		if(ia.password.empty()) {
		    r = false;
		    break;
		} else {
		    conf.setourid(ia);
		}
	    }
	}
    }

    regmode = false;
    return r;
}

void centericq::inithooks() {
    protocolname pname;

    for(pname = icq; pname != protocolname_size; (int) pname += 1) {
	gethook(pname).init();
    }
}

void centericq::mainloop() {
    bool finished = false;
    string text, url;
    int action, old, gid;
    icqcontact *c;
    char buf[512];
    vector<imcontact>::iterator i;

    face.draw();

    while(!finished) {
	face.status(_("F2/M current contact menu, F3/S change status, F4/G general actions, Q quit"));
	c = face.mainloop(action);

	switch(action) {
	    case ACT_IGNORELIST:
		face.modelist(csignore);
		break;
	    case ACT_VISIBLELIST:
		face.modelist(csvisible);
		break;
	    case ACT_INVISLIST:
		face.modelist(csinvisible);
		break;
	    case ACT_STATUS:
		changestatus();
		break;
	    case ACT_ORG_GROUPS:
		face.organizegroups();
		break;
	    case ACT_HIDEOFFLINE:
		conf.sethideoffline(!conf.gethideoffline());
		face.update();
		break;
	    case ACT_DETAILS:
		manager.exec();
		break;
	    case ACT_FIND:
		find();
		break;
	    case ACT_CONF:
		updateconf();
		break;
	    case ACT_QUICKFIND:
		face.quickfind();
		break;
	    case ACT_QUIT:
		finished = true;
		break;
	}

	if(!c) continue;

	switch(action) {
	    case ACT_URL:
		sendevent(imurl(c->getdesc(), imevent::outgoing, "", ""),
		    icqface::ok);
		break;

	    case ACT_SMS:
		sendevent(imsms(c->getdesc(), imevent::outgoing,
		    c->getpostponed()), icqface::ok);
		break;

	    case ACT_IGNORE:
		sprintf(buf, _("Ignore all events from %s?"), c->getdesc().totext().c_str());
		if(face.ask(buf, ASK_YES | ASK_NO, ASK_NO) == ASK_YES) {
		    lst.push_back(modelistitem(c->getdispnick(), c->getdesc(), csignore));
		    clist.remove(c->getdesc());
		    face.update();
		}
		break;

	    case ACT_FILE:
		if(c->getstatus() != offline) {
//                    sendfiles(c->getdesc());
		}
		break;

	    case ACT_EDITUSER:
		c->save();
		if(face.updatedetails(c)) {
		    c->setdispnick(c->getnick());
		    c->save();
		    face.relaxedupdate();
		} else {
		    c->load();
		}
		break;

	    case ACT_ADD:
		if(!c->inlist()) {
		    addcontact(c->getdesc());
		}
		break;

	    case ACT_INFO:
		if(c->getdesc() != contactroot)
		    userinfo(c->getdesc());
		break;

	    case ACT_REMOVE:
		sprintf(buf, _("Are you sure want to remove %s?"), c->getdesc().totext().c_str());
		if(face.ask(buf, ASK_YES | ASK_NO, ASK_NO) == ASK_YES) {
		    clist.remove(c->getdesc());
		    face.update();
		}
		break;

	    case ACT_RENAME:
		text = face.inputstr(_("New nickname to show: "), c->getdispnick());
		if(text.size()) {
		    c->setdispnick(text);
		    face.update();
		}
		break;

	    case ACT_GROUPMOVE:
		if(gid = face.selectgroup(_("Select a group to move the user to"))) {
		    c->setgroupid(gid);
		    face.fillcontactlist();
		}
		break;

	    case ACT_HISTORY:
		history(c->getdesc());
		break;

	    case ACT_MSG:
		if(c->getmsgcount()) {
		    readevents(c->getdesc());
		} else {
		    if(c->getdesc() != contactroot)
		    if(c->getdesc().pname != infocard) {
			sendevent(immessage(c->getdesc(), imevent::outgoing,
			    c->getpostponed()), icqface::ok);
		    }
		}
		break;
	}
    }
}

void centericq::changestatus() {
    imstatus st;
    protocolname pname;

    if(face.changestatus(pname, st)) {
	gethook(pname).setstatus(st);
	face.update();
    }
}

void centericq::find() {
    static imsearchparams s;
    bool ret = true;
    icqcontact *c;

    while(ret) {
	if(ret = face.finddialog(s)) {
	    switch(s.pname) {
		case icq:
//                    ihook.sendsearchreq(s);
//                    ret = face.findresults();
		    if(s.uin) {
			addcontact(imcontact(s.uin, s.pname));
			ret = false;
		    }
		    break;

		case msn:
		    if(s.nick.size() > 12)
		    if(s.nick.substr(s.nick.size()-12) == "@hotmail.com") {
			s.nick.erase(s.nick.size()-12);
		    }

		case yahoo:
		    if(!s.nick.empty()) {
			addcontact(imcontact(s.nick, s.pname));
			ret = false;
		    }
		    break;

		case infocard:
		    c = clist.addnew(imcontact(0, infocard), true);

		    if(face.updatedetails(c)) {
			addcontact(c->getdesc());
		    }

		    if(!c->inlist()) {
			clist.remove(c->getdesc());
		    } else {
			c->save();
		    }

		    ret = false;
		    break;
	    }
	}
    }
}

void centericq::userinfo(const imcontact cinfo) {
    imcontact realuin = cinfo;
    icqcontact *c = clist.get(cinfo);

    if(!c) {
	c = clist.get(contactroot);
	realuin = contactroot;
	c->clear();
	gethook(cinfo.pname).requestinfo(cinfo);
    }

    if(c) {
	face.userinfo(cinfo, realuin);
    }
}

bool centericq::updateconf() {
    bool r;

    regsound snd = rsdontchange;
    regcolor clr = rcdontchange;

    if(r = face.updateconf(snd, clr)) {
	if(snd != rsdontchange) {
	    conf.setregsound(snd);
	    unlink(conf.getconfigfname("sounds").c_str());
	    conf.loadsounds();
	}

	if(clr != rcdontchange) {
	    conf.setregcolor(clr);
	    unlink(conf.getconfigfname("colorscheme").c_str());
	    conf.loadcolors();
	    face.done();
	    face.init();
	    face.draw();
	}

	face.update();
    }

    return r;
}

void centericq::checkmail() {
    static int fsize = -1;
    const char *fname = getenv("MAIL");
    struct stat st;
    int mcount = 0;
    char buf[512];
    string lastfrom;
    bool prevempty, header;
    FILE *f;

    if(conf.getmailcheck() && fname)
    if(!stat(fname, &st))
    if(st.st_size != fsize) {

	if(fsize != -1) {
	    if(f = fopen(fname, "r")) {
		prevempty = header = true;

		while(!feof(f)) {
		    freads(f, buf, 512);

		    if(prevempty && !strncmp(buf, "From ", 5)) {
			lastfrom = strim(buf+5);
			mcount++;
			header = true;
		    } else if(header && !strncmp(buf, "From: ", 6)) {
			lastfrom = strim(buf+6);
		    }

		    if(prevempty = !buf[0]) header = false;
		}
		fclose(f);
	    }

	    if(mcount) {
		face.log(_("+ new mail arrived, %d messages"), mcount);
		face.log(_("+ last msg from %s"), lastfrom.c_str());
		clist.get(contactroot)->playsound(imevent::email);
	    }
	}

	fsize = st.st_size;
    }
}

void centericq::handlesignal(int signum) {
    int status;

    switch(signum) {
	case SIGCHLD:
	    while(wait3(&status, WNOHANG, 0) > 0);
	    break;
	case SIGALRM:
	    break;
    }
}

void centericq::checkparallel() {
    string pidfname = conf.getdirname() + "pid", fname;
    int pid = 0;
    char exename[512];

    ifstream f(pidfname.c_str());
    if(f.is_open()) {
	f >> pid;
	f.close();
    }

    if(!(fname = readlink((string) "/proc/" + i2str(pid) + "/exe")).empty() && (pid != getpid())) {
	if(justfname(fname) == "centericq") {
	    face.log(_("! another running copy of centericq detected"));
	    face.log(_("! this may cause problems. pid %lu"), pid);
	}
    } else {
	pid = 0;
    }

    if(!pid) {
	ofstream of(pidfname.c_str());
	if(of.is_open()) {
	    of << getpid() << endl;
	    of.close();
	}
    }
}

void centericq::sendevent(const imevent &ev, icqface::eventviewresult r) {
    bool proceed;
    string text, fwdnote;
    imevent *sendev;
    icqcontact *c;
    vector<imcontact>::iterator i;

    sendev = 0;
    face.muins.clear();

    if(ev.getdirection() == imevent::incoming) {
	fwdnote = ev.getcontact().totext();
    } else {
	fwdnote = "I";
    }

    fwdnote += " wrote:\n\n";

    if(ev.gettype() == imevent::message) {
	const immessage *m = dynamic_cast<const immessage *>(&ev);

	text = m->gettext();

	if(r == icqface::reply) {
	    if(conf.getquote()) {
		text = quotemsg(text);
	    } else {
		text = "";
	    }
	} else if(r == icqface::forward) {
	    text = fwdnote + text;
	}

	sendev = new immessage(m->getcontact(), imevent::outgoing, text);

    } else if(ev.gettype() == imevent::url) {
	const imurl *m = dynamic_cast<const imurl *>(&ev);

	text = m->getdescription();

	switch(r) {
	    case icqface::forward:
		text = fwdnote + text;
	    case icqface::ok:
		sendev = new imurl(m->getcontact(), imevent::outgoing,
		    m->geturl(), text);
		break;

	    case icqface::reply:
		/* reply to an URL is a message */
		sendev = new immessage(m->getcontact(), imevent::outgoing, "");
		break;
	}
    } else if(ev.gettype() == imevent::sms) {
	const imsms *m = dynamic_cast<const imsms *>(&ev);

	if(c = clist.get(ev.getcontact())) {
	    icqcontact::basicinfo b = c->getbasicinfo();

	    if(b.cellular.find_first_of("0123456789") == -1) {
		b.cellular = face.inputstr(_("Mobile number: "), b.cellular);

		if((b.cellular.find_first_of("0123456789") == -1)
		|| (face.getlastinputkey() == KEY_ESC))
		    return;

		c->setbasicinfo(b);
		c->save();
	    }
	}

	text = m->gettext();

	if(r == icqface::reply) {
	    if(conf.getquote()) {
		text = quotemsg(text);
	    } else {
		text = "";
	    }
	} else if(r == icqface::forward) {
	    text = fwdnote + text;
	}

	sendev = new imsms(m->getcontact(), imevent::outgoing, text);
    }

    if(proceed = sendev) {
	switch(r) {
	    case icqface::forward:
		if(proceed = face.multicontacts())
		    proceed = !face.muins.empty();
		break;
	    case icqface::reply:
	    case icqface::ok:
		face.muins.push_back(ev.getcontact());
		break;
	}

	if(proceed) {
	    if(face.eventedit(*sendev))
	    if(!sendev->empty()) {
		for(i = face.muins.begin(); i != face.muins.end(); i++) {
		    sendev->setcontact(*i);
		    em.store(*sendev);
		}
	    }
	}
    }

    if(sendev) {
	delete sendev;
    }
}

void centericq::readevents(const imcontact &cont) {
    vector<imevent *> events;
    vector<imevent *>::iterator iev;
    icqcontact *c = clist.get(cont);
    icqface::eventviewresult r;
    bool fin, enough;

    if(c) {
	fin = false;

	while(c->getmsgcount() && !fin) {
	    events = em.getevents(cont, c->getlastread());
	    fin = events.empty();

	    for(iev = events.begin(); (iev != events.end()) && !fin; iev++) {
		if((*iev)->getdirection() == imevent::incoming) {
		    enough = false;

		    while(!enough && !fin) {
			r = face.eventview(*iev);

			switch(r) {
			    case icqface::forward:
				sendevent(**iev, r);
				break;

			    case icqface::reply:
				sendevent(**iev, r);
				enough = true;
				break;

			    case icqface::ok:
				enough = true;
				break;

			    case icqface::cancel:
				fin = true;
				break;

			    case icqface::open:
				if(const imurl *m = static_cast<const imurl *>(*iev))
				    conf.openurl(m->geturl());
				break;

			    case icqface::accept:
				em.store(imauthorization(cont,
				    imevent::outgoing, true, "accepted"));
				enough = true;
				break;

			    case icqface::reject:
				em.store(imauthorization(cont,
				    imevent::outgoing, false, "rejected"));
				enough = true;
				break;
			}
		    }
		}

		c->setlastread((*iev)->gettimestamp());
	    }

	    while(!events.empty()) {
		delete *events.begin();
		events.erase(events.begin());
	    }
	}

	c->save();
	face.update();
    }
}

void centericq::history(const imcontact &cont) {
    vector<imevent *> events;
    icqface::eventviewresult r;
    imevent *im;
    bool enough;

    events = em.getevents(cont, 0);

    if(!events.empty()) {
	face.histmake(events);

	while(face.histexec(im)) {
	    enough = false;

	    while(!enough) {
		r = face.eventview(im);

		switch(r) {
		    case icqface::forward:
		    case icqface::reply:
			sendevent(*im, r);
			break;

		    case icqface::accept:
			enough = true;
			em.store(imauthorization(cont, imevent::outgoing,
			    true, "accepted"));
			break;

		    case icqface::reject:
			enough = true;
			em.store(imauthorization(cont, imevent::outgoing,
			    false, "rejected"));
			break;

		    case icqface::cancel:
		    case icqface::ok:
			enough = true;
			break;

		    case icqface::open:
			if(const imurl *m = static_cast<const imurl *>(im))
			    conf.openurl(m->geturl());
			break;
		}
	    }
	}

	face.status("");

	while(!events.empty()) {
	    delete *events.begin();
	    events.erase(events.begin());
	}

    } else {
	face.log(_("+ no history items for %s"), cont.totext().c_str());
    }
}

const string centericq::quotemsg(const string text) {
    string ret;
    vector<string> lines;
    vector<string>::iterator i;

    breakintolines(text, lines, WORKAREA_X2-WORKAREA_X1-4);

    for(i = lines.begin(); i != lines.end(); i++) {
	if(!i->empty()) ret += (string) "> " + *i;
	ret = trailcut(ret);
	ret += "\n";
    }

    return ret;
}

icqcontact *centericq::addcontact(const imcontact ic) {
    icqcontact *c;
    int groupid = 0;

    if(c = clist.get(ic)) {
	if(c->inlist()) {
	    face.log(_("+ user %s is already on the list"), ic.totext().c_str());
	    return 0;
	}
    }

    if(conf.getusegroups()) {
	groupid = face.selectgroup(_("Select a group to add the user to"));
	if(!groupid) return 0;
    }

    abstracthook &hook = gethook(ic.pname);

    if(hook.getcapabilities() & hoptCanNotify) {
	if(face.ask(_("Notify the user he/she has been added?"),
	ASK_YES | ASK_NO, ASK_YES) == ASK_YES) {
//            ihook.sendalert(ic);
	    face.log(_("+ the notification has been sent to %lu"), ic.uin);
	}
    }

    if(!c) {
	c = clist.addnew(ic, false);
    } else {
	c->includeintolist();
    }

    hook.sendnewuser(c->getdesc());
    c->setgroupid(groupid);
    face.log(_("+ %s has been added to the list"), ic.totext().c_str());
    face.update();

    return c;
}

bool centericq::idle(int options = 0) {
    bool keypressed, online, fin;
    fd_set fds;
    struct timeval tv;
    int hsockfd;
    protocolname pname;

    for(keypressed = fin = false; !keypressed && !fin; ) {
//        timer_keypress = lastkeypress();

	FD_ZERO(&fds);
	FD_SET(hsockfd = 0, &fds);
	online = false;

	if(!regmode) {
	    exectimers();

	    for(pname = icq; pname != protocolname_size; (int) pname += 1) {
		abstracthook &hook = gethook(pname);

		if(hook.online()) {
		    hook.getsockets(fds, hsockfd);
		    online = true;
		}
	    }
	}

	tv.tv_sec = online ? PERIOD_SELECT : PERIOD_RECONNECT/3;
	tv.tv_usec = 0;

	select(hsockfd+1, &fds, 0, 0, &tv);

	if(FD_ISSET(0, &fds)) {
	    keypressed = true;
	    time(&timer_keypress);
	} else
	for(pname = icq; pname != protocolname_size; (int) pname += 1) {
	    abstracthook &hook = gethook(pname);

	    if(hook.online())
	    if(hook.isoursocket(fds)) {
		hook.main();
		fin = fin || (options & HIDL_SOCKEXIT);
	    }
	}
    }

    return keypressed;
}

void centericq::setauto(imstatus astatus) {
    protocolname pname;
    imstatus stcurrent;
    static bool autoset = false;

    if((astatus == available) && !autoset)
	return;

    for(pname = icq; pname != protocolname_size; (int) pname += 1) {
	abstracthook &hook = gethook(pname);
	stcurrent = hook.getstatus();

	if(hook.logged())
	switch(stcurrent) {
	    case invisible:
	    case offline:
		break;

	    default:
		if(autoset && (astatus == available)) {
		    face.log(_("+ the user is back"));
		    hook.restorestatus();
		    autoset = false;
		} else {
		    if(astatus == away && stcurrent == notavail)
			break;

		    if(!autoset && (astatus != stcurrent)) {
			hook.setautostatus(astatus);
			autoset = true;

			face.log(_("+ [%s] automatically set %s"),
			    conf.getprotocolname(pname).c_str(),
			    astatus == away ? _("away") : _("n/a"));
		    }
		}
		break;
	}
    }
}

#define MINCK0(x, y)       (x ? (y ? (x > y ? y : x) : x) : y)

void centericq::exectimers() {
    time_t timer_current = time(0);
    protocolname pname;
    int paway, pna;

    for(pname = icq; pname != protocolname_size; (int) pname += 1) {
	if(!conf.getourid(pname).empty()) {
	    gethook(pname).exectimers();
	}
    }

    conf.getauto(paway, pna);

    if(paway || pna) {
	if(paway && (timer_current-timer_keypress > paway*60))
	    setauto(away);

	if(pna && (timer_current-timer_keypress > pna*60))
	    setauto(notavail);

	if(timer_current-timer_keypress < MINCK0(paway, pna)*60)
	    setauto(available);
    }

    if(timer_current-timer_checkmail > PERIOD_CHECKMAIL) {
	cicq.checkmail();
	time(&timer_checkmail);
    }

    if(timer_current-timer_resend > PERIOD_RESEND) {
	em.resend();
	face.relaxedupdate();
	time(&timer_resend);
    }

    if(face.updaterequested())
    if(timer_current-timer_update > PERIOD_DISPUPDATE) {
	face.update();
	time(&timer_update);
    }
}

// ----------------------------------------------------------------------------

const string rusconv(const string tdir, const string text) {
    static unsigned char kw[] = {
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,184,164,165,166,167,168,169,170,171,172,173,174,175,
	176,177,178,168,180,181,182,183,184,185,186,187,188,189,190,191,
	254,224,225,246,228,229,244,227,245,232,233,234,235,236,237,238,
	239,255,240,241,242,243,230,226,252,251,231,248,253,249,247,250,
	222,192,193,214,196,197,212,195,213,200,201,202,203,204,205,206,
	207,223,208,209,210,211,198,194,220,219,199,216,221,217,215,218
    };

    static unsigned char wk[] = {
	128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
	160,161,162,163,164,165,166,167,179,169,170,171,172,173,174,175,
	176,177,178,179,180,181,182,183,163,185,186,187,188,189,190,191,
	225,226,247,231,228,229,246,250,233,234,235,236,237,238,239,240,
	242,243,244,245,230,232,227,254,251,253,255,249,248,252,224,241,
	193,194,215,199,196,197,214,218,201,202,203,204,205,206,207,208,
	210,211,212,213,198,200,195,222,219,221,223,217,216,220,192,209
    };

    string r;
    unsigned char c;
    string::const_iterator i;
    unsigned char *table = 0;

    if(tdir == "kw") table = kw; else
    if(tdir == "wk") table = wk;

    if(conf.getrussian() && table) {
	for(i = text.begin(); i != text.end(); i++) {
	    c = (unsigned char) *i;
	    c &= 0377;
	    if(c & 0200) c = table[c & 0177];
	    r += c;
	}
    } else {
	r = text;
    }

    return r;
}
