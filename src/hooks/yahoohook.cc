#include "yahoohook.h"
#include "icqmlist.h"
#include "icqface.h"
#include "icqcontacts.h"
#include "icqoffline.h"
#include "accountmanager.h"

yahoohook::yahoohook() {
    fonline = false;
    timer_reconnect = 0;
    yahoo = 0;
    manualstatus = conf.getstatus(::yahoo);
}

yahoohook::~yahoohook() {
    conf.savestatus(::yahoo, manualstatus);
}

void yahoohook::init(const icqconf::imaccount account) {
    yahoo_options options;

    memset(&options, 0, sizeof(options));
    options.connect_mode = YAHOO_CONNECT_NORMAL;

    face.log("+ initializing %s engine",
	conf.getprotocolname(account.pname).c_str());

    if(yahoo = yahoo_init(account.nickname.c_str(), account.password.c_str(),
    &options)) {
	yahoo->yahoo_Disconnected = &disconnected;
	yahoo->yahoo_UserLogon = &userlogon;
	yahoo->yahoo_UserLogoff = &userlogoff;
	yahoo->yahoo_UserStatus = &userstatus;
	yahoo->yahoo_RecvBounced = &recvbounced;
	yahoo->yahoo_RecvMessage = &recvmessage;

#ifdef DEBUG
	yahoo->yahoo_Log = &log;
#else
	yahoo->yahoo_Log = 0;
#endif
    } else {
	face.log("+ unable to init %s engine",
	    conf.getprotocolname(account.pname).c_str());
    }
}

void yahoohook::connect() {
    icqconf::imaccount acc = conf.getourid(::yahoo);

    if(!enabled()) init(acc);

    if(yahoo) {
	face.log(_("+ [yahoo] connecting to the server"));

	alarm(5);

	if(!yahoo_connect(yahoo)) {
	    face.log(_("+ [yahoo] unable to connect to the server"));
	    disconnected(yahoo);
	} else {
	    yahoo_get_config(yahoo);
	    if(!yahoo_cmd_logon(yahoo, ourstatus = YAHOO_STATUS_AVAILABLE)) {
		fonline = true;
		face.log(_("+ [yahoo] logged in"));
		clist.send();
		offl.scan(0, ossendall);
	    }
	}
    }

    time(&timer_reconnect);
}

void yahoohook::main() {
    if(yahoo) {
	yahoo_main(yahoo);
    }
}

int yahoohook::getsockfd() const {
    return (yahoo && fonline) ? yahoo->sockfd : 0;
}

void yahoohook::disconnect() {
    if(yahoo) {
	yahoo_cmd_logoff(yahoo);
	disconnected(yahoo);
    }
}

void yahoohook::exectimers() {
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

struct tm *yahoohook::timestamp() {
    time_t t = time(0);
    return localtime(&t);
}

unsigned long yahoohook::sendmessage(const icqcontact *c, const string atext) {
    string text = atext;
    string::iterator is;

    for(is = text.begin(); is != text.end(); is++)
	if((unsigned) *is < 32) *is = ' ';

    if(c->getstatus() != offline) {
	yahoo_cmd_msg(yahoo, conf.getourid(::yahoo).nickname.c_str(),
	    c->getdesc().nickname.c_str(), text.c_str());
    } else {
	yahoo_cmd_msg_offline(yahoo, conf.getourid(::yahoo).nickname.c_str(),
	    c->getdesc().nickname.c_str(), text.c_str());
    }

    return 1;
}

bool yahoohook::online() const {
    return fonline;
}

bool yahoohook::logged() const {
    return fonline;
}

void yahoohook::sendnewuser(const imcontact ic) {
    char *group;

    if(yahoo)
    if(online()) {
	if(yahoo->buddies && yahoo->buddies[0]) {
	    group = yahoo->buddies[0]->group;
	} else {
	    group = "group";
	}

	if(!yahoo_isbuddy(yahoo, ic.nickname.c_str())) {
	    face.log(_("+ [yahoo] adding %s to the contacts"), ic.nickname.c_str());
	    yahoo_add_buddy(yahoo, ic.nickname.c_str(),
		conf.getourid(::yahoo).nickname.c_str(), group, "");
	}
    }
}

void yahoohook::removeuser(const imcontact ic) {
    if(yahoo)
    if(online()) {
	face.log(_("+ [yahoo] removing %s from the contacts"), ic.nickname.c_str());
	yahoo_remove_buddy(yahoo, ic.nickname.c_str(),
	    conf.getourid(::yahoo).nickname.c_str(), "group", "");
    }
}

imstatus yahoohook::yahoo2imstatus(int status) const {
    imstatus st = offline;

    switch(status) {
	case YAHOO_STATUS_AVAILABLE:
	case YAHOO_STATUS_CUSTOM:
	    st = available;
	    break;
	case YAHOO_STATUS_BUSY:
	    st = occupied;
	    break;
	case YAHOO_STATUS_BRB:
	case YAHOO_STATUS_IDLE:
	case YAHOO_STATUS_ONPHONE:
	    st = away;
	    break;
	case YAHOO_STATUS_NOTATHOME:
	case YAHOO_STATUS_NOTATDESK:
	case YAHOO_STATUS_NOTINOFFICE:
	case YAHOO_STATUS_ONVACATION:
	case YAHOO_STATUS_OUTTOLUNCH:
	case YAHOO_STATUS_STEPPEDOUT:
	    st = notavail;
	    break;
	case YAHOO_STATUS_INVISIBLE:
	    st = invisible;
	    break;
	case -1:
	    st = offline;
	    break;
    }

    return st;
}

bool yahoohook::enabled() const {
    return (bool) yahoo;
}

void yahoohook::setautostatus(imstatus st) {
    static int stat2int[imstatus_size] = {
	0,
	YAHOO_STATUS_AVAILABLE,
	YAHOO_STATUS_INVISIBLE,
	YAHOO_STATUS_CUSTOM,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_BUSY,
	YAHOO_STATUS_NOTATHOME,
	YAHOO_STATUS_IDLE
    };

    if(st == offline) {
	if(getstatus() != offline) {
	    disconnect();
	}
    } else {
	if(getstatus() == offline) {
	    connect();
	} else {
	    ourstatus = stat2int[st];

	    if(st == available) {
		yahoo_cmd_set_back_mode(yahoo, ourstatus, "available");
	    } else {
		if(ourstatus == YAHOO_STATUS_IDLE) {
		    yahoo_cmd_idle(yahoo);
		} else {
		    yahoo_cmd_set_away_mode(yahoo, ourstatus, "away");
		}
	    }
	}
    }
}

void yahoohook::setstatus(imstatus st) {
    setautostatus(manualstatus = st);
}

imstatus yahoohook::getstatus() const {
    return online() ? yahoo2imstatus(ourstatus) : offline;
}

// ----------------------------------------------------------------------------

void yahoohook::disconnected(yahoo_context *y) {
    int i;
    icqcontact *c;

    if(yhook.fonline) {
	face.log(_("+ [yahoo] disconnected from the network"));
	yhook.fonline = false;
    }

    for(i = 0; i < clist.count; i++) {
	c = (icqcontact *) clist.at(i);
	if(c->getdesc().pname == ::yahoo) {
	    c->setstatus(offline);
	}
    }

    time(&yhook.timer_reconnect);
}

void yahoohook::userlogon(yahoo_context *y, const char *nick, int status) {
    yhook.userstatus(yhook.yahoo, nick, status);
}

void yahoohook::userlogoff(yahoo_context *y, const char *nick) {
    yhook.userstatus(yhook.yahoo, nick, -1);
}

void yahoohook::userstatus(yahoo_context *y, const char *nick, int status) {
    imcontact ic(nick, ::yahoo);
    icqcontact *c = clist.get(ic);

    if(!c) {
	c = clist.addnew(ic, false);
    }

    c->setstatus(yhook.yahoo2imstatus(status));

    if(c->getstatus() != offline) {
	c->setlastseen();
    }

    face.update();
}

void yahoohook::recvbounced(yahoo_context *y, const char *nick) {
    face.log(_("+ [yahoo] bounced msg to %s"), nick);
}

void yahoohook::recvmessage(yahoo_context *y, const char *nick, const char *msg) {
    imcontact ic(nick, ::yahoo);

    if(strlen(msg))
    if(!lst.inlist(ic, csignore)) {
	hist.putmessage(ic, msg, HIST_MSG_IN, timestamp());
	icqcontact *c = clist.get(ic);

	if(c) {
	    if(c->getstatus() == offline) {
		c->setstatus(available);
	    }

	    c->setmsgcount(c->getmsgcount()+1);
	    c->playsound(EVT_MSG);
	    face.update();
	}
    }
}

void yahoohook::log(yahoo_context *y, const char *msg) {
    face.log(msg);
}