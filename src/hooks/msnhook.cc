/*
*
* centericq MSN protocol handling class
* $Id: msnhook.cc,v 1.9 2001/12/06 16:56:34 konst Exp $
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

#include "msnhook.h"
#include "icqconf.h"
#include "icqface.h"
#include "icqcontacts.h"
#include "accountmanager.h"
#include "eventmanager.h"

msnhook mhook;

#define TIMESTAMP maketm(d->hour-1, d->minute, d->day, d->month, d->year)

msnhook::msnhook() {
    status = offline;
    fonline = false;
    timer_reconnect = 0;

    for(int i = MSN_RNG; i != MSN_NUM_EVENTS; i++) {
	msn_event[i] = 0;
    }

    MSN_RegisterCallback(MSN_MSG, &messaged);
    MSN_RegisterCallback(MSN_ILN, &statuschanged);
    MSN_RegisterCallback(MSN_NLN, &statuschanged);
    MSN_RegisterCallback(MSN_FLN, &statuschanged);
    MSN_RegisterCallback(MSN_AUTH, &authrequested);
    MSN_RegisterCallback(MSN_OUT, &disconnected);
    MSN_RegisterCallback(MSN_RNG, &ring);
//    MSN_RegisterCallback(MSN_MAIL, &mail_callback);

#ifdef DEBUG
    MSN_RegisterCallback(MSN_LOG, &log);
#endif
}

msnhook::~msnhook() {
}

void msnhook::init() {
    manualstatus = conf.getstatus(msn);
}

void msnhook::connect() {
    icqconf::imaccount account = conf.getourid(msn);

    face.log(_("+ [msn] connecting to the server"));

    if(!MSN_Login(account.nickname.c_str(), account.password.c_str(),
    DEFAULT_HOST, DEFAULT_PORT)) {
	face.log(_("+ [msn] logged in"));
	fonline = true;
	status = available;
	setautostatus(manualstatus);
    } else {
	face.log(_("+ [msn] unable to connect to the server"));
    }

    time(&mhook.timer_reconnect);
    msn_Russian = 0;
}

void msnhook::disconnect() {
    if(online()) {
	disconnected(0);
    }
}

void msnhook::exectimers() {
    time_t tcurrent = time(0);

    if(logged()) {
    } else {
	if(tcurrent-timer_reconnect > PERIOD_RECONNECT) {
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

void msnhook::main() {
    if(online()) {
	MSN_Main();
    }
}

void msnhook::getsockets(fd_set &fds, int &hsocket) const {
    vector<int> sockets = MSN_GetSockets();
    vector<int>::iterator i;

    for(i = sockets.begin(); i != sockets.end(); i++) {
	hsocket = max(hsocket, *i);
	FD_SET(*i, &fds);
    }
}

bool msnhook::isoursocket(fd_set &fds) const {
    vector<int> sockets = MSN_GetSockets();
    vector<int>::iterator i;

    for(i = sockets.begin(); i != sockets.end(); i++) {
	if(FD_ISSET(*i, &fds))
	    return true;
    }

    return false;
}

bool msnhook::online() const {
    return fonline;
}

bool msnhook::logged() const {
    return fonline;
}

bool msnhook::isconnecting() const {
    return false;
}

bool msnhook::enabled() const {
    return true;
}

bool msnhook::send(const imevent &ev) {
    if(ev.gettype() == imevent::message) {
	const immessage *m = static_cast<const immessage *>(&ev);
	icqcontact *c = clist.get(ev.getcontact());

	if(c && m)
	if(c->getstatus() != offline) {
	    MSN_SendMessage(ev.getcontact().nickname.c_str(), m->gettext().c_str());
	    return true;
	}
    }

    return false;
}

void msnhook::sendnewuser(const imcontact c) {
    if(logged()) {
	MSN_AddContact(c.nickname.c_str());
    }
}

void msnhook::setautostatus(imstatus st) {
    static int stat2int[imstatus_size] = {
	USER_FLN,
	USER_NLN,
	USER_HDN,
	USER_NLN,
	USER_BSY,
	USER_BSY,
	USER_AWY,
	USER_IDL
    };

    if(st != offline) {
	if(getstatus() == offline) {
	    connect();
	} else {
	    MSN_ChangeState(stat2int[status = st]);
	}
    } else {
	if(getstatus() != offline) {
	    disconnect();
	}
    }
}

imstatus msnhook::getstatus() const {
    return status;
}

imstatus msnhook::msn2imstatus(int st) const {
    switch(st) {
	case USER_HDN:
	    return invisible;

	case USER_BSY:
	case USER_PHN:
	    return occupied;

	case USER_IDL:
	case USER_BRB:
	case USER_AWY:
	    return away;

	case USER_LUN:
	    return notavail;

	case USER_FLN:
	    return offline;

	case USER_NLN:
	default:
	    return available;
    }
}

void msnhook::removeuser(const imcontact ic) {
    face.log(_("+ [msn] removing %s from the contacts"), ic.nickname.c_str());
    MSN_RemoveContact(ic.nickname.c_str());
}

// ----------------------------------------------------------------------------

void msnhook::messaged(void *data) {
    MSN_InstantMessage *d = (MSN_InstantMessage *) data;
    imcontact ic(d->sender, msn);
    icqcontact *c;

    if(strlen(d->msg)) {
	c = clist.get(ic);

	if(!c)
	if(c = clist.addnew(ic))
	if(d->friendlyhandle) {
	    c->setdispnick(d->friendlyhandle);
	}

	em.store(immessage(ic, imevent::incoming, d->msg));

	if(c)
	if(c->getstatus() == offline) {
	    c->setstatus(available);
	}
    }
}

void msnhook::statuschanged(void *data) {
    MSN_StatusChange *d = (MSN_StatusChange *) data;
    imcontact ic(d->handle, msn);
    icqcontact *c = clist.get(ic);

    if(!c) {
	c = clist.addnew(ic, false);
	if(d->friendlyhandle) {
	    c->setdispnick(d->friendlyhandle);
	}
    }

    c->setstatus(mhook.msn2imstatus(d->newStatus));

    if(c->getstatus() != offline) {
	c->setlastseen();
    }
}

void msnhook::authrequested(void *data) {
    MSN_AuthMessage *msg = (MSN_AuthMessage *) data;
    MSN_AuthorizeContact(msg->conn, msg->requestor);
}

void msnhook::disconnected(void *data) {
    int i;
    icqcontact *c;

    if(mhook.online()) {
	face.log(_("+ [msn] disconnected"));
	mhook.fonline = false;
    }

    clist.setoffline(msn);
    mhook.status = offline;
    time(&mhook.timer_reconnect);
    face.update();
}

void msnhook::log(void *data) {
    face.log("%s", (const char *) data);
}

void msnhook::ring(void *data) {
    MSN_Ring *ring = (MSN_Ring *) data;

    if(ring->mode == outgoing) {
	face.log(_("+ [msn] connecting with %s"), ring->handle.c_str());
    } else {
	face.log(_("+ [msn] connection from %s"), ring->handle.c_str());
    }
}