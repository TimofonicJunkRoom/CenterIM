/*
*
* centericq icq protocol handling class
* $Id: icqhook.cc,v 1.69 2002/03/15 15:15:38 konst Exp $
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

#include "icqhook.h"
#include "icqconf.h"
#include "icqface.h"
#include "icqcontacts.h"
#include "accountmanager.h"
#include "centericq.h"
#include "imlogger.h"

#include <libicq2000/userinfohelpers.h>

#define PERIOD_ICQPOLL  5

icqhook ihook;

static const Status stat2int[imstatus_size] = {
    STATUS_OFFLINE,
    STATUS_ONLINE,
    STATUS_ONLINE, /* invisible */
    STATUS_FREEFORCHAT,
    STATUS_DND,
    STATUS_OCCUPIED,
    STATUS_NA,
    STATUS_AWAY
};

icqhook::icqhook() {
    timer_reconnect = 0;
    fonline = false;

    fcapabilities =
	hoptCanSendURL |
	hoptCanSendSMS |
	hoptCanSetAwayMsg |
	hoptCanChangeNick |
	hoptCanUpdateDetails |
	hoptChangableServer;

    cli.connected.connect(slot(this, &icqhook::connected_cb));
    cli.disconnected.connect(slot(this, &icqhook::disconnected_cb));
    cli.socket.connect(slot(this, &icqhook::socket_cb));
    cli.messaged.connect(slot(this, &icqhook::messaged_cb));
    cli.messageack.connect(slot(this, &icqhook::messageack_cb));
    cli.contactlist.connect(slot(this, &icqhook::contactlist_cb));
    cli.newuin.connect(slot(this, &icqhook::newuin_cb));
    cli.rate.connect(slot(this, &icqhook::rate_cb));
    cli.want_auto_resp.connect(slot(this, &icqhook::want_auto_resp_cb));
    cli.search_result.connect(slot(this, &icqhook::search_result_cb));
    cli.self_event.connect(slot(this, &icqhook::self_event_cb));

#ifdef DEBUG
    cli.logger.connect(slot(this, &icqhook::logger_cb));
#endif
}

icqhook::~icqhook() {
    cli.logger.clear();
    cli.connected.clear();
    cli.disconnected.clear();
    cli.socket.clear();
    cli.messaged.clear();
    cli.messageack.clear();
    cli.contactlist.clear();
    cli.newuin.clear();
    cli.rate.clear();
    cli.self_event.clear();
    cli.search_result.clear();
    cli.want_auto_resp.clear();
}

void icqhook::init() {
    manualstatus = conf.getstatus(icq);
}

void icqhook::connect() {
    icqconf::imaccount acc = conf.getourid(icq);
    int i;
    icqcontact *c;

    for(i = 0; i < clist.count; i++) {
	c = (icqcontact *) clist.at(i);

	if(c->getdesc().pname == icq) {
	    Contact ic(c->getdesc().uin);
	    cli.addContact(ic);
	}
    }

    if(!acc.server.empty()) cli.setLoginServerHost(acc.server);
    if(acc.port) cli.setLoginServerPort(acc.port);

    if(!conf.getsmtphost().empty()) cli.setSMTPServerHost(conf.getsmtphost());
    if(conf.getsmtpport()) cli.setSMTPServerPort(conf.getsmtpport());

    cli.setUIN(acc.uin);
    cli.setPassword(acc.password);

    face.log(_("+ [icq] connecting to the server"));

    sendinvisible();
    cli.setStatus(stat2int[manualstatus], manualstatus == invisible);
    logger.putourstatus(icq, offline, manualstatus);

    time(&timer_reconnect);
    fonline = true;
    flogged = false;
}

void icqhook::disconnect() {
    logger.putourstatus(icq, getstatus(), offline);
    cli.setStatus(STATUS_OFFLINE);
}

void icqhook::resolve() {
    int i;
    icqcontact *c;
    imcontact cont;

    for(i = 0; i < clist.count; i++) {
	c = (icqcontact *) clist.at(i);
	cont = c->getdesc();

	if(cont.pname == icq)
	if(c->getdispnick() == i2str(c->getdesc().uin)) {
	    requestinfo(cont);
	}
    }
}

void icqhook::sendinvisible() {
    icqlist::iterator i;
    Contact *c;
    int k;
/*
    for(k = 0; k < clist.count; k++) {
	if(c = cli.getContact(((icqcontact *) clist.at(k))->getdesc().uin)) {
	    c->setInvisible(false);
	}
    }

    for(i = lst.begin(); i != lst.end(); i++) {
	switch(i->getstatus()) {
	    case csinvisible:
		if(c = cli.getContact(i->getdesc().uin)) {
		    c->setInvisible(true);
		}
		break;
	}
    }
*/
}

void icqhook::exectimers() {
    time_t tcurrent = time(0);

    if(logged()) {
	if(tcurrent-timer_poll > PERIOD_ICQPOLL) {
	    cli.Poll();
	    sendinvisible();
	    time(&timer_poll);
	}
    } else {
	if(tcurrent-timer_reconnect > PERIOD_RECONNECT && conf.enoughdiskspace()) {
	    if(online() && !logged()) {
		disconnect();
	    } else if(manualstatus != offline) {
		if(!manager.isopen()) {
		    connect();
		}
	    }
	}
    }
}

void icqhook::main() {
    vector<int>::iterator i;
    struct timeval tv;
    int hsock;
    fd_set rs, ws, es;

    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);

    tv.tv_sec = tv.tv_usec = 0;
    hsock = 0;

    for(i = rfds.begin(); i != rfds.end(); i++) {
	FD_SET(*i, &rs);
	hsock = max(hsock, *i);
    }

    for(i = wfds.begin(); i != wfds.end(); i++) {
	FD_SET(*i, &ws);
	hsock = max(hsock, *i);
    }

    for(i = efds.begin(); i != efds.end(); i++) {
	FD_SET(*i, &es);
	hsock = max(hsock, *i);
    }

    if(select(hsock+1, &rs, &ws, &es, &tv) > 0) {
	for(i = rfds.begin(); i != rfds.end(); i++) {
	    if(FD_ISSET(*i, &rs)) {
		cli.socket_cb(*i, SocketEvent::READ);
		break;
	    }
	}

	for(i = wfds.begin(); i != wfds.end(); i++) {
	    if(FD_ISSET(*i, &ws)) {
		cli.socket_cb(*i, SocketEvent::WRITE);
		break;
	    }
	}

	for(i = efds.begin(); i != efds.end(); i++) {
	    if(FD_ISSET(*i, &es)) {
		cli.socket_cb(*i, SocketEvent::EXCEPTION);
		break;
	    }
	}
    }
}

void icqhook::getsockets(fd_set &rs, fd_set &ws, fd_set &es, int &hsocket) const {
    vector<int>::const_iterator i;

    for(i = rfds.begin(); i != rfds.end(); i++) {
	hsocket = max(hsocket, *i);
	FD_SET(*i, &rs);
    }

    for(i = wfds.begin(); i != wfds.end(); i++) {
	hsocket = max(hsocket, *i);
	FD_SET(*i, &ws);
    }

    for(i = efds.begin(); i != efds.end(); i++) {
	hsocket = max(hsocket, *i);
	FD_SET(*i, &es);
    }
}

bool icqhook::isoursocket(fd_set &rs, fd_set &ws, fd_set &es) const {
    vector<int>::const_iterator i;

    for(i = rfds.begin(); i != rfds.end(); i++)
	if(FD_ISSET(*i, &rs))
	    return true;

    for(i = wfds.begin(); i != wfds.end(); i++)
	if(FD_ISSET(*i, &ws))
	    return true;

    for(i = efds.begin(); i != efds.end(); i++)
	if(FD_ISSET(*i, &es))
	    return true;

    return false;
}

bool icqhook::online() const {
    return fonline;
}

bool icqhook::logged() const {
    return fonline && flogged;
}

bool icqhook::isconnecting() const {
    return fonline && !flogged;
}

bool icqhook::enabled() const {
    return true;
}

bool icqhook::send(const imevent &ev) {
    unsigned int uin = ev.getcontact().uin;
    Contact *ic = cli.getContact(uin);

    if(!ic) {
	return false;
    }

    if(ev.gettype() == imevent::message) {
	const immessage *m = static_cast<const immessage *> (&ev);
	cli.SendEvent(new NormalMessageEvent(ic, rusconv("kw", m->gettext())));

    } else if(ev.gettype() == imevent::url) {
	const imurl *m = static_cast<const imurl *> (&ev);
	cli.SendEvent(new URLMessageEvent(ic,
	    rusconv("kw", m->getdescription()),
	    rusconv("kw", m->geturl())));

    } else if(ev.gettype() == imevent::sms) {
	const imsms *m = static_cast<const imsms *> (&ev);
	ic->setMobileNo(clist.get(ev.getcontact())->getbasicinfo().cellular);
	cli.SendEvent(new SMSMessageEvent(ic, rusconv("kw", m->gettext()), true));

    } else if(ev.gettype() == imevent::authorization) {
	const imauthorization *m = static_cast<const imauthorization *> (&ev);
	cli.SendEvent(new AuthAckEvent(ic,
	    rusconv("kw", m->gettext()), m->getgranted()));

    } else {
	return false;
    }

    return true;
}

void icqhook::sendnewuser(const imcontact &c) {
    if(logged()) {
	Contact ic(c.uin);
	cli.addContact(ic);
	cli.fetchDetailContactInfo(cli.getContact(c.uin));
    }
}

void icqhook::setautostatus(imstatus st) {
    if(st != offline) {
	if(getstatus() == offline) {
	    connect();
	} else {
	    sendinvisible();
	    logger.putourstatus(icq, getstatus(), st);
	    cli.setStatus(stat2int[st], st == invisible);
	}
    } else {
	if(getstatus() != offline) {
	    disconnect();
	}
    }
}

imstatus icqhook::getstatus() const {
    if(!fonline) {
	return offline;
    } else if(cli.getInvisible()) {
	return invisible;
    } else {
	return icq2imstatus(cli.getStatus());
    }
}

bool icqhook::regconnect(const string &aserv) {
    int pos;

    if((pos = aserv.find(":")) != -1) {
	cli.setLoginServerHost(aserv.substr(0, pos));
	cli.setLoginServerPort(strtoul(aserv.substr(pos+1).c_str(), 0, 0));
    }

    return true;
}

bool icqhook::regattempt(unsigned int &auin, const string &apassword) {
    fd_set srfds, swfds, sefds;
    struct timeval tv;
    int hsockfd;
    time_t regtimeout = time(0);

    reguin = 0;

    cli.setPassword(apassword);
    cli.RegisterUIN();

    while(!reguin && (time(0)-regtimeout < 60)) {
	hsockfd = 0;

	FD_ZERO(&srfds);
	FD_ZERO(&swfds);
	FD_ZERO(&sefds);

	getsockets(srfds, swfds, sefds, hsockfd);

	tv.tv_sec = 30;
	tv.tv_usec = 0;

	select(hsockfd+1, &srfds, &swfds, &sefds, &tv);

	if(isoursocket(srfds, swfds, sefds)) {
	    main();
	}
    }

    auin = reguin;
    return (bool) reguin;
}

imstatus icqhook::icq2imstatus(const Status st) const {
    switch(st) {
	case STATUS_AWAY:
	    return away;
	case STATUS_NA:
	    return notavail;
	case STATUS_OCCUPIED:
	    return occupied;
	case STATUS_DND:
	    return dontdisturb;
	case STATUS_FREEFORCHAT:
	    return freeforchat;
	case STATUS_OFFLINE:
	    return offline;
	case STATUS_ONLINE:
	default:
	    return available;
    }
}

void icqhook::requestinfo(const imcontact &c) {
    if(logged()) {
	if(c == imcontact(conf.getourid(icq).uin, icq)) {
	    // Our info is requested
	    cli.fetchSelfDetailContactInfo();
	} else {
	    Contact *icont = cli.getContact(c.uin);

	    if(!icont) {
		Contact ic(c.uin);
		cli.addContact(ic);
	    } else {
		cli.fetchDetailContactInfo(icont);
	    }
	}
    }
}

void icqhook::lookup(const imsearchparams &params, verticalmenu &dest) {
    ICQ2000::Sex sex;

    searchdest = &dest;

    sex = params.gender == genderMale ? ICQ2000::SEX_MALE :
	params.gender == genderFemale ? ICQ2000::SEX_FEMALE :
	ICQ2000::SEX_UNSPECIFIED;

    if(!params.email.empty() || params.minage || params.maxage ||
    !params.city.empty() || !params.state.empty() ||
    !params.company.empty() || !params.department.empty() ||
    !params.position.empty() || params.onlineonly ||
    params.country || params.language ||
    (sex != ICQ2000::SEX_UNSPECIFIED) || (params.agerange != range_NoRange)) {
	searchevent = cli.searchForContacts(params.nick, params.firstname,
	    params.lastname, params.email, params.agerange, sex,
	    params.language, params.city, params.state, params.country,
	    params.company, params.department, params.position,
	    params.onlineonly);
    } else {
	searchevent = cli.searchForContacts(params.nick, params.firstname,
	    params.lastname);
    }
}

void icqhook::sendupdateuserinfo(const icqcontact &c) {
    Contact *ic = cli.getSelfContact();

    MainHomeInfo &home = ic->getMainHomeInfo();
    HomepageInfo &hpage = ic->getHomepageInfo();
    WorkInfo &work = ic->getWorkInfo();

    icqcontact::basicinfo cbinfo = c.getbasicinfo();
    icqcontact::moreinfo cminfo = c.getmoreinfo();
    icqcontact::workinfo cwinfo = c.getworkinfo();

    /* basic information */

    ic->setAlias(rusconv("kw", c.getnick()));
    ic->setFirstName(rusconv("kw", cbinfo.fname));
    ic->setLastName(rusconv("kw", cbinfo.lname));
    ic->setEmail(rusconv("kw", cbinfo.email));
    ic->setAuthReq(cbinfo.requiresauth);

    ic->setAboutInfo(rusconv("kw", c.getabout()));

    home.city = rusconv("kw", cbinfo.city);
    home.state = rusconv("kw", cbinfo.state);
    home.phone = rusconv("kw", cbinfo.phone);
    home.fax = rusconv("kw", cbinfo.fax);
    home.street = rusconv("kw", cbinfo.street);
    home.setMobileNo(rusconv("kw", cbinfo.cellular));

    home.zip = cbinfo.zip;
    home.country = cbinfo.country;
    home.timezone = UserInfoHelpers::getSystemTimezone();

    /* more information */

    hpage.age = cminfo.age;

    hpage.sex =
	cminfo.gender == genderFemale ? SEX_FEMALE :
	cminfo.gender == genderMale ? SEX_MALE :
	    SEX_UNSPECIFIED;

    hpage.homepage = rusconv("kw", cminfo.homepage);
    hpage.birth_day = cminfo.birth_day;
    hpage.birth_month = cminfo.birth_month;
    hpage.birth_year = cminfo.birth_year;

    hpage.lang1 = cminfo.lang1;
    hpage.lang2 = cminfo.lang2;
    hpage.lang3 = cminfo.lang3;

    /* work information */

    work.city = rusconv("kw", cwinfo.city);
    work.state = rusconv("kw", cwinfo.state);
    work.street = rusconv("kw", cwinfo.street);
    work.company_name = rusconv("kw", cwinfo.company);
    work.company_dept = rusconv("kw", cwinfo.dept);
    work.company_position = rusconv("kw", cwinfo.position);
    work.company_web = rusconv("kw", cwinfo.homepage);

    work.zip = cwinfo.zip;
    work.country = cwinfo.country;

    ic->setMainHomeInfo(home);
    ic->setHomepageInfo(hpage);
    ic->setWorkInfo(work);

    cli.uploadSelfDetails();
}

void icqhook::requestawaymsg(const imcontact &c) {
    Contact *ic = cli.getContact(c.uin);

    if(ic) {
	MessageEvent *ev = new AwayMessageEvent(ic);
	cli.SendEvent(ev);
    }
}

void icqhook::updateinforecord(Contact *ic, icqcontact *c) {
    if(ic && c) {
	MainHomeInfo &home = ic->getMainHomeInfo();
	HomepageInfo &hpage = ic->getHomepageInfo();
	WorkInfo &work = ic->getWorkInfo();
	PersonalInterestInfo &pint = ic->getPersonalInterestInfo();
	BackgroundInfo &backg = ic->getBackgroundInfo();
	EmailInfo &email = ic->getEmailInfo();

	icqcontact::basicinfo cbinfo = c->getbasicinfo();
	icqcontact::moreinfo cminfo = c->getmoreinfo();
	icqcontact::workinfo cwinfo = c->getworkinfo();

	/* basic information */

	cbinfo.fname = rusconv("wk", ic->getFirstName());
	cbinfo.lname = rusconv("wk", ic->getLastName());
	cbinfo.email = rusconv("wk", ic->getEmail());
	cbinfo.city = rusconv("wk", home.city);
	cbinfo.state = rusconv("wk", home.state);
	cbinfo.phone = rusconv("wk", home.phone);
	cbinfo.fax = rusconv("wk", home.fax);
	cbinfo.street = rusconv("wk", home.street);
	cbinfo.requiresauth = ic->getAuthReq();

	if(!home.getMobileNo().empty())
	    cbinfo.cellular = rusconv("wk", home.getMobileNo());

	cbinfo.zip = home.zip;
	cbinfo.country = home.country;

	/* more information */

	cminfo.age = hpage.age;

	cminfo.gender =
	    hpage.sex == 1 ? genderFemale :
	    hpage.sex == 2 ? genderMale :
		genderUnspec;

	cminfo.homepage = rusconv("wk", hpage.homepage);
	cminfo.birth_day = hpage.birth_day;
	cminfo.birth_month = hpage.birth_month;
	cminfo.birth_year = hpage.birth_year;

	cminfo.lang1 = hpage.lang1;
	cminfo.lang2 = hpage.lang2;
	cminfo.lang3 = hpage.lang3;

	cminfo.timezone = home.timezone;

	/* work information */

	cwinfo.city = rusconv("wk", work.city);
	cwinfo.state = rusconv("wk", work.state);
	cwinfo.street = rusconv("wk", work.street);
	cwinfo.company = rusconv("wk", work.company_name);
	cwinfo.dept = rusconv("wk", work.company_dept);
	cwinfo.position = rusconv("wk", work.company_position);
	cwinfo.homepage = rusconv("wk", work.company_web);

	cwinfo.zip = work.zip;
	cwinfo.country = work.country;

	/* personal interests */

	string sbuf;
	vector<string> pintinfo;
	list<PersonalInterestInfo::Interest>::iterator ii;

	for(ii = pint.interests.begin(); ii != pint.interests.end(); ii++) {
	    sbuf = UserInfoHelpers::getInterestsIDtoString(ii->first);

	    if(!ii->second.empty()) {
		if(!sbuf.empty()) sbuf += ": ";
		sbuf += rusconv("wk", ii->second);
	    }

	    if(!sbuf.empty()) pintinfo.push_back(sbuf);
	}

	/* education background */

	vector<string> backginfo;
	list<BackgroundInfo::School>::iterator isc;

	for(isc = backg.schools.begin(); isc != backg.schools.end(); isc++) {
	    sbuf = UserInfoHelpers::getBackgroundIDtoString(isc->first);
	    if(!sbuf.empty()) sbuf += ": ";
	    sbuf += rusconv("wk", isc->second);
	    if(!sbuf.empty()) backginfo.push_back(sbuf);
	}

	/* nicknames stuff */

	string nick = rusconv("wk", ic->getAlias());

	if((c->getnick() == c->getdispnick())
	|| (c->getdispnick() == i2str(c->getdesc().uin))) {
	    c->setdispnick(nick);
	}

	c->setnick(nick);
	c->setbasicinfo(cbinfo);
	c->setmoreinfo(cminfo);
	c->setworkinfo(cwinfo);
	c->setinterests(pintinfo);
	c->setbackground(backginfo);
	c->setabout(rusconv("wk", ic->getAboutInfo()));

	face.relaxedupdate();
    }
}

// ----------------------------------------------------------------------------

void icqhook::connected_cb(ConnectedEvent *ev) {
    flogged = true;

    time(&timer_poll);

    face.log(_("+ [icq] logged in"));
    face.update();

    string buf;
    ifstream f(conf.getconfigfname("icq-infoset").c_str());

    if(f.is_open()) {
	getstring(f, buf); cli.getSelfContact()->setAlias(buf);
	getstring(f, buf); cli.getSelfContact()->setEmail(buf);
	getstring(f, buf); cli.getSelfContact()->setFirstName(buf);
	getstring(f, buf); cli.getSelfContact()->setLastName(buf);
	f.close();

	cli.getSelfContact()->getMainHomeInfo().timezone = UserInfoHelpers::getSystemTimezone();
	cli.uploadSelfDetails();

	unlink(conf.getconfigfname("icq-infoset").c_str());
    }

    resolve();
}

void icqhook::disconnected_cb(DisconnectedEvent *ev) {
    string msg;

    static const string reasons[DisconnectedEvent::FAILED_UNKNOWN+1] = {
	_("as requested"),
	_("socket problems"),
	_("bad username"),
	_("turboing"),
	_("bad password"),
	_("username and password mismatch"),
	_("already logged"),
	""
    };

    rfds.clear();
    wfds.clear();
    efds.clear();

    clist.setoffline(icq);
    fonline = false;

    msg = _("+ [icq] disconnected");
    if(!reasons[ev->getReason()].empty()) {
	msg += ", ";
	msg += reasons[ev->getReason()];
    }

    face.log(msg);
    face.update();
}

bool icqhook::messaged_cb(MessageEvent *ev) {
    imcontact ic;
    time_t t;

    ic = imcontact(ev->getContact()->getUIN(), icq);

    if(ev->getType() == MessageEvent::Normal) {
	NormalMessageEvent *r;
	if(r = dynamic_cast<NormalMessageEvent *>(ev)) {
	    em.store(immessage(ic, imevent::incoming,
		rusconv("wk", r->getMessage())));
	}

    } else if(ev->getType() == MessageEvent::URL) {
	URLMessageEvent *r;
	if(r = dynamic_cast<URLMessageEvent *>(ev)) {
	    em.store(imurl(ic, imevent::incoming,
		rusconv("wk", r->getURL()),
		rusconv("wk", r->getMessage())));
	}

    } else if(ev->getType() == MessageEvent::SMS) {
	SMSMessageEvent *r;

	if(r = dynamic_cast<SMSMessageEvent *>(ev)) {
	    icqcontact *c = clist.getmobile(r->getSender());

	    if(!c)
	    if(c = clist.addnew(imcontact(0, infocard), true)) {
		icqcontact::basicinfo b = c->getbasicinfo();
		b.cellular = r->getSender();

		c->setbasicinfo(b);

		c->setdispnick(b.cellular);
		c->setnick(b.cellular);
	    }

	    if(c) {
		em.store(imsms(c->getdesc(), imevent::incoming,
		    rusconv("wk", r->getMessage())));
	    }
	}

    } else if(ev->getType() == MessageEvent::AuthReq) {
	AuthReqEvent *r;
	if(r = dynamic_cast<AuthReqEvent *>(ev)) {
	    em.store(imauthorization(ic, imevent::incoming, false,
		rusconv("wk", r->getMessage())));
	}

    } else if(ev->getType() == MessageEvent::AuthAck) {
    } else if(ev->getType() == MessageEvent::EmailEx) {
	EmailExEvent *r;

	if(r = dynamic_cast<EmailExEvent *>(ev)) {
	    icqcontact *c = clist.getemail(r->getEmail());

	    if(!c)
	    if(c = clist.addnew(imcontact(0, infocard), true)) {
		c->setdispnick(r->getEmail());
		c->setnick(r->getEmail());

		icqcontact::basicinfo b = c->getbasicinfo();

		if(r->getSender() != r->getEmail()) {
		    int pos = r->getSender().find(" ");

		    if(pos != -1) {
			b.fname = r->getSender().substr(0, pos);
			b.lname = r->getSender().substr(pos+1);
		    } else {
			b.fname = r->getSender();
		    }
		}

		b.email = r->getEmail();
		c->setbasicinfo(b);
	    }

	    if(c) {
		em.store(imemail(c->getdesc(), imevent::incoming,
		    rusconv("wk", r->getMessage())));
	    }
	}

    } else if(ev->getType() == MessageEvent::UserAdd) {
	UserAddEvent *r;
	if(r = dynamic_cast<UserAddEvent *>(ev)) {
	    em.store(immessage(ic, imevent::incoming,
		string() + _("* The user has added you to his/her contact list")));
	}

    }

    return true;
}

void icqhook::messageack_cb(MessageEvent *ev) {
    imcontact ic;
    icqcontact *c;

    ic = imcontact(ev->getContact()->getUIN(), icq);
    c = clist.get(ic);

    if(ev->isFinished()) {
	/*
	*
	* The FINAL decision.
	*
	*/
	
	switch(ev->getType()) {
	    case MessageEvent::SMS:
		if(!ev->isDelivered()) {
		    face.log(_("+ [icq] failed SMS to %s, %s"),
			c->getdispnick().c_str(), ic.totext().c_str());
		}
		break;

	    case MessageEvent::AwayMessage:
		if(ev->isDelivered()) {
		    AwayMessageEvent *r;

		    if(r = dynamic_cast<AwayMessageEvent *>(ev)) {
			em.store(immessage(ic, imevent::incoming,
			    string() + _("* Away message:") + "\n\n" +
			    rusconv("wk", r->getMessage())));
		    }
		} else {
		    face.log(_("+ [icq] cannot fetch away msg from %s, %s"),
			c->getdispnick().c_str(), ic.totext().c_str());
		}
		break;
	}
    } else {
	/*
	*
	* Rejected, but there is still something to be done.
	*
	*/
    }
}

void icqhook::contactlist_cb(ContactListEvent *ev) {
    icqcontact *c = clist.get(imcontact(ev->getUIN(), icq));
    char buf[64];
    string lastip, sbuf;
    int ip;

    switch(ev->getType()) {
	case ContactListEvent::StatusChange:
	    if(dynamic_cast<StatusChangeEvent*>(ev))
	    if(c) {
		Contact *ic = cli.getContact(ev->getUIN());
		StatusChangeEvent *tev = dynamic_cast<StatusChangeEvent*>(ev);

		logger.putonline(c->getdesc(),
		    c->getstatus(),
		    icq2imstatus(tev->getStatus()));

		c->setstatus(icq2imstatus(tev->getStatus()));

		if(c->getstatus() != offline) {
		    c->setlastseen();

		    if(inet_ntop(AF_INET, &(ip = ntohl(ic->getExtIP())), buf, 64)) {
			lastip = buf;
		    }

		    if(lastip.find_first_not_of(".0") != -1) {
			if(inet_ntop(AF_INET, &(ip = ntohl(ic->getLanIP())), buf, 64)) {
			    sbuf = buf;

			    if(sbuf.find_first_not_of(".0") != -1) {
				if(lastip != sbuf) {
				    if(!lastip.empty()) lastip += " ";
				    lastip += sbuf;
				}
			    }
			}

			c->setlastip(lastip);
		    }
		}
	    }
	    break;

	case ContactListEvent::UserInfoChange:
	    if(dynamic_cast<UserInfoChangeEvent*>(ev)) {
		if(!c) {
		    c = clist.get(contactroot);
		    /*
		    *
		    * In case we wanna look up info about a user which
		    * is not on our contact list.
		    *
		    */
		}

		updateinforecord(cli.getContact(ev->getUIN()), c);
	    }
	    break;
    }
}

void icqhook::newuin_cb(NewUINEvent *ev) {
    reguin = ev->getUIN();
}

void icqhook::rate_cb(RateInfoChangeEvent *ev) {
}

void icqhook::logger_cb(LogEvent *ev) {
    switch(ev->getType()) {
	case LogEvent::PACKET:
	case LogEvent::DIRECTPACKET:
#if PACKETDEBUG
	    face.log(ev->getMessage());
#endif
	    break;

	default:
	    face.log(ev->getMessage());
	    break;
    }
}

void icqhook::socket_cb(SocketEvent *ev) {
    vector<int>::iterator i;

    if(dynamic_cast<AddSocketHandleEvent*>(ev)) {
	AddSocketHandleEvent *cev = dynamic_cast<AddSocketHandleEvent*>(ev);

	if(cev->isRead()) rfds.push_back(cev->getSocketHandle());
	if(cev->isWrite()) wfds.push_back(cev->getSocketHandle());
	if(cev->isException()) efds.push_back(cev->getSocketHandle());

    } else if(dynamic_cast<RemoveSocketHandleEvent*>(ev)) {
	RemoveSocketHandleEvent *cev = dynamic_cast<RemoveSocketHandleEvent*>(ev);

	i = find(rfds.begin(), rfds.end(), cev->getSocketHandle());
	if(i != rfds.end())
	    rfds.erase(i);

	i = find(wfds.begin(), wfds.end(), cev->getSocketHandle());
	if(i != wfds.end())
	    wfds.erase(i);

	i = find(efds.begin(), efds.end(), cev->getSocketHandle());
	if(i != efds.end())
	    efds.erase(i);
    }
}

void icqhook::want_auto_resp_cb(AwayMessageEvent *ev) {
    char buf[128];

    sprintf(buf, _("%lu requested our icq away message, sent the response"), ev->getSenderUIN());
    logger.putmessage(buf);

    ev->setMessage(conf.getawaymsg(icq));
}

void icqhook::search_result_cb(SearchResultEvent *ev) {
    if(ev == searchevent) {
	string line;
	Contact *c = ev->getLastContactAdded();

	if(searchdest && c) {
	    line = (c->getStatus() == STATUS_ONLINE ? "o " : "  ") +
		rusconv("wk", c->getAlias());

	    if(line.size() > 12) line.resize(12);
	    else line += string(12-line.size(), ' ');

	    line += " " + 
		rusconv("wk", c->getFirstName()) + " " +
		rusconv("wk", c->getLastName());

	    if(!c->getEmail().empty())
		line += " <" + rusconv("wk", c->getEmail()) + ">";

	    searchdest->additem(0, c->getUIN(), line);

	    searchdest->redraw();
	}

	if(ev->isFinished()) {
	    face.log(_("+ [icq] whitepages search finished, %d found"),
		ev->getContactList().size());
	}
    }
}

void icqhook::self_event_cb(SelfEvent *ev) {
    switch(ev->getType()) {
	case SelfEvent::MyStatusChange:
	    face.update();
	    break;

	case SelfEvent::MyUserInfoChange:
	    updateinforecord(cli.getSelfContact(), clist.get(contactroot));
	    updateinforecord(cli.getSelfContact(), clist.get(imcontact(cli.getUIN(), icq)));
	    break;
    }
}
