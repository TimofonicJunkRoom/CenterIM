/*
*
* centericq AIM protocol handling class
* $Id: aimhook.cc,v 1.11 2002/03/26 12:52:01 konst Exp $
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

#include "aimhook.h"
#include "icqface.h"
#include "accountmanager.h"
#include "icqcontacts.h"
#include "centericq.h"
#include "imlogger.h"

#ifdef DEBUG
#define DLOG(s) face.log("aim %s", s)
#else
#define DLOG(s)
#endif

aimhook ahook;

aimhook::aimhook()
    : handle(firetalk_create_handle(FP_AIMTOC, 0)),
      fonline(false), flogged(false), ourstatus(offline)
{
    fcapabilities =
	hoptCanSetAwayMsg |
	hoptCanChangeNick |
	hoptCanChangePassword |
	hoptCanUpdateDetails |
	hoptChangableServer;
}

aimhook::~aimhook() {
}

void aimhook::init() {
    manualstatus = conf.getstatus(aim);

    firetalk_register_callback(handle, FC_CONNECTED, &connected);
    firetalk_register_callback(handle, FC_CONNECTFAILED, &connectfailed);
    firetalk_register_callback(handle, FC_DISCONNECT, &disconnected);
    firetalk_register_callback(handle, FC_NEWNICK, &newnick);
    firetalk_register_callback(handle, FC_PASSCHANGED, &newpass);
    firetalk_register_callback(handle, FC_IM_GOTINFO, &gotinfo);
    firetalk_register_callback(handle, FC_IM_GETMESSAGE, &getmessage);
    firetalk_register_callback(handle, FC_IM_GETACTION, &getmessage);
    firetalk_register_callback(handle, FC_IM_BUDDYONLINE, &buddyonline);
    firetalk_register_callback(handle, FC_IM_BUDDYOFFLINE, &buddyoffline);
    firetalk_register_callback(handle, FC_IM_BUDDYAWAY, &buddyaway);
    firetalk_register_callback(handle, FC_IM_BUDDYUNAWAY, &buddyonline);
    firetalk_register_callback(handle, FC_NEEDPASS, &needpass);
    firetalk_register_callback(handle, FC_IM_USER_NICKCHANGED, &buddynickchanged);
}

void aimhook::connect() {
    icqconf::imaccount acc = conf.getourid(aim);

    face.log(_("+ [aim] connecting to the server"));

    fonline = firetalk_signon(handle, acc.server.c_str(), acc.port, acc.nickname.c_str()) == FE_SUCCESS;

    if(fonline) {
	flogged = false;
    } else {
	face.log(_("+ [aim] unable to connect to the server"));
    }
}

void aimhook::disconnect() {
    if(ahook.fonline) {
	firetalk_disconnect(ahook.handle);
    }
}

void aimhook::exectimers() {
}

void aimhook::main() {
    firetalk_select();
}

void aimhook::getsockets(fd_set &rfds, fd_set &wfds, fd_set &efds, int &hsocket) const {
    int *r, *w, *e, *sr, *sw, *se;

    firetalk_getsockets(&sr, &sw, &se);

    for(r = sr; *r; r++) {
	if(*r > hsocket) hsocket = *r;
	FD_SET(*r, &rfds);
    }

    for(w = sw; *w; w++) {
	if(*w > hsocket) hsocket = *w;
	FD_SET(*w, &wfds);
    }

    for(e = se; *e; e++) {
	if(*e > hsocket) hsocket = *e;
	FD_SET(*e, &efds);
    }

    delete sr;
    delete sw;
    delete se;
}

bool aimhook::isoursocket(fd_set &rfds, fd_set &wfds, fd_set &efds) const {
    bool res = false;
    int *r, *w, *e, *sr, *sw, *se;

    if(online()) {
	firetalk_getsockets(&sr, &sw, &se);

	for(r = sr; *r; r++) res = res || FD_ISSET(*r, &rfds);
	for(w = sw; *w; w++) res = res || FD_ISSET(*w, &wfds);
	for(e = se; *e; e++) res = res || FD_ISSET(*e, &efds);

	delete sr;
	delete sw;
	delete se;
    }

    return res;
}

bool aimhook::online() const {
    return fonline;
}

bool aimhook::logged() const {
    return flogged && fonline;
}

bool aimhook::isconnecting() const {
    return fonline && !flogged;
}

bool aimhook::enabled() const {
    return true;
}

bool aimhook::send(const imevent &ev) {
    icqcontact *c = clist.get(ev.getcontact());
    string text;

    if(c) {
	if(ev.gettype() == imevent::message) {
	    const immessage *m = static_cast<const immessage *>(&ev);
	    if(m) text = rusconv("kw", m->gettext());

	} else if(ev.gettype() == imevent::url) {
	    const imurl *m = static_cast<const imurl *>(&ev);
	    if(m) text = rusconv("kw", m->geturl()) + "\n\n" + rusconv("kw", m->getdescription());

	}

	return firetalk_im_send_message(handle, c->getdesc().nickname.c_str(), text.c_str(), 0) == FE_SUCCESS;
    }

    return false;
}

void aimhook::sendnewuser(const imcontact &ic) {
    if(online()) {
	face.log(_("+ [aim] adding %s to the contacts"), ic.nickname.c_str());
	firetalk_im_add_buddy(handle, ic.nickname.c_str());
	firetalk_im_upload_buddies(handle);
	requestinfo(ic);
    }
}

void aimhook::removeuser(const imcontact &ic) {
    if(online()) {
	face.log(_("+ [aim] removing %s from the contacts"), ic.nickname.c_str());
	firetalk_im_remove_buddy(handle, ic.nickname.c_str());
	firetalk_im_upload_buddies(handle);
    }
}

void aimhook::setautostatus(imstatus st) {
    if(st != offline) {
	if(getstatus() == offline) {
	    connect();
	} else {
	    if(getstatus() != st) {
		logger.putourstatus(aim, getstatus(), ourstatus = st);

		switch(st) {
		    case away:
		    case notavail:
		    case occupied:
			firetalk_set_away(ahook.handle,
			    conf.getawaymsg(aim).c_str());
			break;

		    default:
			firetalk_set_away(ahook.handle, 0);
			break;
		}
	    }
	}
    } else {
	if(getstatus() != offline) {
	    disconnect();
	}
    }

    face.update();
}

imstatus aimhook::getstatus() const {
    return (flogged && fonline) ? ourstatus : offline;
}

void aimhook::requestinfo(const imcontact &c) {
    if(c != imcontact(conf.getourid(aim).uin, aim)) {
	firetalk_im_get_info(handle, c.nickname.c_str());
    } else {
	/*
	*
	* Our own details set has been requested
	*
	*/

	icqcontact *cc = clist.get(contactroot);
	icqconf::imaccount acc = conf.getourid(aim);

	loadprofile();

	cc->setabout(profile.info);
	cc->setnick(acc.nickname);
    }
}

void aimhook::sendupdateuserinfo(icqcontact &c, const string &newpass) {
    icqconf::imaccount acc = conf.getourid(aim);

    if(!c.getnick().empty())
    if(c.getnick() != acc.nickname) {
	firetalk_set_nickname(handle, c.getnick().c_str());
    }

    if(!newpass.empty())
    if(newpass != acc.password) {
	firetalk_set_password(handle, acc.password.c_str(), newpass.c_str());
    }

    profile.info = c.getabout();
    firetalk_set_info(ahook.handle, ahook.profile.info.c_str());
    saveprofile();
}

void aimhook::saveprofile() {
    ofstream f((conf.getconfigfname("aim-profile")).c_str());
    if(f.is_open()) {
	f << profile.info;
	f.close();
    }
}

void aimhook::loadprofile() {
    string buf, fname;

    profile = ourprofile();
    fname = conf.getconfigfname("aim-profile");

    if(access(fname.c_str(), R_OK)) {
	char sbuf[512];
	sprintf(sbuf, _("I do really enjoy the default AIM profile of centericq %s."), VERSION);
	profile.info = sbuf;
	saveprofile();
    }

    ifstream f(fname.c_str());

    if(f.is_open()) {
	while(!f.eof()) {
	    getstring(f, buf);
	    if(!profile.info.empty()) profile.info += "\n";
	    profile.info += buf;
	}
    }
}

void aimhook::resolve() {
    int i;
    icqcontact *c;
    imcontact cont;

    for(i = 0; i < clist.count; i++) {
	c = (icqcontact *) clist.at(i);
	cont = c->getdesc();

	if(cont.pname == aim)
	if(c->getabout().empty()) {
	    requestinfo(cont);
	}
    }
}

// ----------------------------------------------------------------------------

void aimhook::connected(void *connection, void *cli, ...) {
    ahook.flogged = true;
    face.log(_("+ [aim] logged in"));
    face.update();

    int i;
    icqcontact *c;

    for(i = 0; i < clist.count; i++) {
	c = (icqcontact *) clist.at(i);

	if(c->getdesc().pname == aim) {
	    firetalk_im_add_buddy(ahook.handle, c->getdesc().nickname.c_str());
	}
    }

    firetalk_im_upload_buddies(ahook.handle);

    ahook.ourstatus = available;
    ahook.setautostatus(ahook.manualstatus);

    ahook.loadprofile();
    firetalk_set_info(ahook.handle, ahook.profile.info.c_str());

    ahook.resolve();
    DLOG("connected");
}

void aimhook::disconnected(void *connection, void *cli, ...) {
    ahook.fonline = false;
    logger.putourstatus(aim, ahook.getstatus(), offline);
    clist.setoffline(aim);

    face.log(_("+ [aim] disconnected from the network"));
    face.update();
    DLOG("disconnected");
}

void aimhook::connectfailed(void *connection, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    int err = va_arg(ap, int);
    char *reason = va_arg(ap, char *);
    va_end(ap);

    ahook.fonline = false;

    face.log(_("+ [aim] connect failed: %s"), reason);
    face.update();
    DLOG("connectfailed");
}

void aimhook::newnick(void *connection, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *nick = va_arg(ap, char *);
    va_end(ap);

    if(nick)
    if(strlen(nick)) {
	icqconf::imaccount acc = conf.getourid(aim);
	acc.nickname = nick;
	conf.setourid(acc);

	face.log(_("+ [aim] nickname was changed successfully"));
    }

    DLOG("newnick");
}

void aimhook::newpass(void *connection, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *pass = va_arg(ap, char *);
    va_end(ap);

    if(pass)
    if(strlen(pass)) {
	icqconf::imaccount acc = conf.getourid(aim);
	acc.password = pass;
	conf.setourid(acc);

	face.log(_("+ [aim] password was changed successfully"));
    }

    DLOG("newpass");
}

void aimhook::gotinfo(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *nick = va_arg(ap, char *);
    char *info = va_arg(ap, char *);
    int warning = va_arg(ap, int);
    int idle = va_arg(ap, int);
    va_end(ap);

    if(nick && info)
    if(strlen(nick) && strlen(info)) {
	icqcontact *c = clist.get(imcontact(nick, aim));

	if(c) {
	    c->setabout(cuthtml(info, true));
	    if(c->getabout().empty())
		c->setabout(_("The user has no profile information."));
	}
    }

    DLOG("gotinfo");
}

void aimhook::getmessage(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *sender = va_arg(ap, char *);
    int automessage_flag = va_arg(ap, int);
    char *message = va_arg(ap, char *);
    va_end(ap);

    if(sender && message)
    if(strlen(sender) && strlen(message)) {
	em.store(immessage(imcontact(sender, aim),
	    imevent::incoming, rusconv("wk", cuthtml(message, true))));
    }

    DLOG("getmessage");
}

void aimhook::userstatus(const string &nickname, imstatus st) {
    if(!nickname.empty()) {
	imcontact ic(nickname, aim);
	icqcontact *c = clist.get(ic);

	if(!c) {
	    c = clist.addnew(ic, false);
	}

	if(st != c->getstatus()) {
	    logger.putonline(ic, c->getstatus(), st);
	    c->setstatus(st);

	    if(c->getstatus() != offline) {
		c->setlastseen();
	    }
	}
    }
}

void aimhook::buddyonline(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *nick = va_arg(ap, char *);
    va_end(ap);

    if(nick)
    if(strlen(nick)) {
	ahook.userstatus(nick, available);
    }
}

void aimhook::buddyoffline(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *nick = va_arg(ap, char *);
    va_end(ap);

    if(nick)
    if(strlen(nick)) {
	ahook.userstatus(nick, offline);
    }
}

void aimhook::buddyaway(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *nick = va_arg(ap, char *);
    va_end(ap);

    if(nick)
    if(strlen(nick)) {
	ahook.userstatus(nick, away);
    }
}

void aimhook::needpass(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *pass = va_arg(ap, char *);
    int size = va_arg(ap, int);
    va_end(ap);

    if(pass) {
	icqconf::imaccount acc = conf.getourid(aim);
	strncpy(pass, acc.password.c_str(), size-1);
	pass[size-1] = 0;
	face.log(_("+ [aim] password sent"));
    }
}

void aimhook::buddynickchanged(void *conn, void *cli, ...) {
    va_list ap;

    va_start(ap, cli);
    char *oldnick = va_arg(ap, char *);
    char *newnick = va_arg(ap, char *);
    va_end(ap);

    if(oldnick && newnick)
    if(strlen(oldnick) && strlen(newnick)) {
	icqcontact *c = clist.get(imcontact(oldnick, aim));

	if(c) {
	    if(c->getnick() == c->getdispnick()) {
		c->setdispnick(newnick);
	    }

	    c->setnick(newnick);

	    face.log(_("+ [aim] user %s changed their nick to %s"),
		oldnick, newnick);
	}
    }
}