/*
*
* centericq Jabber protocol handling class
* $Id: jabberhook.cc,v 1.8 2002/11/22 19:11:59 konst Exp $
*
* Copyright (C) 2002 by Konstantin Klyagin <konst@konst.org.ua>
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

#include "jabberhook.h"
#include "icqface.h"
#include "imlogger.h"
#include "eventmanager.h"

jabberhook jhook;

jabberhook::jabberhook(): jc(0), flogged(false) {
    fcapabs.insert(hookcapab::setaway);
    fcapabs.insert(hookcapab::fetchaway);
    fcapabs.insert(hookcapab::authrequests);
}

jabberhook::~jabberhook() {
}

void jabberhook::init() {
    manualstatus = conf.getstatus(jabber);
}

void jabberhook::connect() {
    icqconf::imaccount acc = conf.getourid(jabber);
    string jid;
    int pos;

    face.log(_("+ [jab] connecting to the server"));
    jid = acc.nickname + "@" + acc.server + "/centericq";

    if((pos = jid.find(":")) != -1)
	jid.erase(pos);

    auto_ptr<char> cjid(strdup(jid.c_str()));
    auto_ptr<char> cpass(strdup(acc.password.c_str()));

    regmode = false;

    jc = jab_new(cjid.get(), cpass.get());

    jab_packet_handler(jc, &packethandler);
    jab_state_handler(jc, &statehandler);

#if PACKETDEBUG
    jab_logger(jc, &jlogger);
#endif

    jab_start(jc);
    id = atoi(jab_auth(jc));

    if(!jc || jc->state == JCONN_STATE_OFF) {
	face.log(_("+ [jab] unable to connect to the server"));
	jab_delete(jc);
	jc = 0;
    }
}

void jabberhook::disconnect() {
    statehandler(jc, JCONN_STATE_OFF);
    jab_delete(jc);
    jc = 0;
}

void jabberhook::main() {
    jab_poll(jc, 0);

    if(!jc) {
	statehandler(jc, JCONN_STATE_OFF);

    } else if(jc->state == JCONN_STATE_OFF || jc->fd == -1) {
	statehandler(jc, JCONN_STATE_OFF);
	jab_delete(jc);
	jc = 0;
    }
}

void jabberhook::getsockets(fd_set &rfds, fd_set &wfds, fd_set &efds, int &hsocket) const {
    if(jc) {
	FD_SET(jc->fd, &rfds);
	hsocket = max(jc->fd, hsocket);
    }
}

bool jabberhook::isoursocket(fd_set &rfds, fd_set &wfds, fd_set &efds) const {
    if(jc) {
	return FD_ISSET(jc->fd, &rfds);
    } else {
	return false;
    }
}

bool jabberhook::online() const {
    return (bool) jc;
}

bool jabberhook::logged() const {
    return jc && flogged;
}

bool jabberhook::isconnecting() const {
    return jc && !flogged;
}

bool jabberhook::enabled() const {
    return true;
}

bool jabberhook::send(const imevent &ev) {
    icqcontact *c = clist.get(ev.getcontact());

    string text;

    if(c) {
	if(ev.gettype() == imevent::message) {
	    const immessage *m = static_cast<const immessage *>(&ev);
	    text = m->gettext();

	} else if(ev.gettype() == imevent::url) {
	    const imurl *m = static_cast<const imurl *>(&ev);
	    text = m->geturl() + "\n\n" + m->getdescription();

	} else if(ev.gettype() == imevent::authorization) {
	    const imauthorization *m = static_cast<const imauthorization *> (&ev);
	    auto_ptr<char> cjid(strdup(jidnormalize(ev.getcontact().nickname).c_str()));
	    xmlnode x = 0;

	    if(m->getauthtype() == imauthorization::Granted) {
		x = jutil_presnew(JPACKET__SUBSCRIBED, cjid.get(), 0);

	    } else if(m->getauthtype() == imauthorization::Request) {
		x = jutil_presnew(JPACKET__SUBSCRIBE, cjid.get(), 0);

	    }

	    if(x) {
		jab_send(jc, x);
		xmlnode_free(x);
	    }

	    return true;
	}

	auto_ptr<char> cjid(strdup(jidnormalize(c->getdesc().nickname).c_str()));
	auto_ptr<char> ctext(strdup(text.c_str()));

	xmlnode x;
	x = jutil_msgnew(TMSG_CHAT, cjid.get(), 0, ctext.get());
	jab_send(jc, x);
	xmlnode_free(x);

	return true;
    }

    return false;
}

void jabberhook::sendnewuser(const imcontact &ic) {
    sendnewuser(ic, true);
}

void jabberhook::removeuser(const imcontact &ic) {
    removeuser(ic, true);
}

void jabberhook::sendnewuser(const imcontact &ic, bool report) {
    xmlnode x, y, z;
    auto_ptr<char> cjid(strdup(jidnormalize(ic.nickname).c_str()));

    if(find(roster.begin(), roster.end(), cjid.get()) != roster.end())
	return;

    roster.push_back(cjid.get());

    if(report)
	face.log(_("+ [jab] adding %s to the contacts"), ic.nickname.c_str());

    x = jutil_presnew(JPACKET__SUBSCRIBE, cjid.get(), 0);
    jab_send(jc, x);
    xmlnode_free(x);

    x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
    y = xmlnode_get_tag(x, "query");
    z = xmlnode_insert_tag(y, "item");
    xmlnode_put_attrib(z, "jid", cjid.get());
    jab_send(jc, x);
    xmlnode_free(x);
}

void jabberhook::removeuser(const imcontact &ic, bool report) {
    xmlnode x, y, z;
    auto_ptr<char> cjid(strdup(jidnormalize(ic.nickname).c_str()));

    vector<string>::iterator ir = find(roster.begin(), roster.end(), cjid.get());
    if(ir == roster.end()) return;
    roster.erase(ir);

    if(report)
	face.log(_("+ [jab] removing %s from the contacts"), ic.nickname.c_str());

    x = jutil_presnew(JPACKET__UNSUBSCRIBE, cjid.get(), 0);

    jab_send(jc, x);
    xmlnode_free(x);

    x = jutil_iqnew(JPACKET__SET, NS_ROSTER);
    y = xmlnode_get_tag(x, "query");
    z = xmlnode_insert_tag(y, "item");
    xmlnode_put_attrib(z, "jid", cjid.get());
    xmlnode_put_attrib(z, "subscription", "remove");
    jab_send(jc, x);
    xmlnode_free(x);
}

void jabberhook::setautostatus(imstatus st) {
    if(st != offline) {
	if(getstatus() == offline) {
	    connect();
	} else {
	    setjabberstatus(ourstatus = st,
		(st == away || st == notavail) ? conf.getawaymsg(jabber) : "");
	}
    } else {
	if(getstatus() != offline) {
	    disconnect();
	}
    }
}

void jabberhook::requestinfo(const imcontact &c) {
}

void jabberhook::requestawaymsg(const imcontact &ic) {
    icqcontact *c = clist.get(ic);

    if(c) {
	string am = awaymsgs[ic.nickname];

	if(!am.empty()) {
	    em.store(imnotification(ic, (string) _("Away message:") + "\n\n" + am));
	} else {
	    face.log(_("+ [jab] no away message from %s, %s"),
		c->getdispnick().c_str(), ic.totext().c_str());
	}
    }
}

imstatus jabberhook::getstatus() const {
    return online() ? ourstatus : offline;
}

bool jabberhook::regnick(const string &nick, const string &pass,
const string &serv, string &err) {
    int pos;
    string jid = nick + "@" + serv;
    if((pos = jid.find(":")) != -1) jid.erase(pos);

    auto_ptr<char> cjid(strdup(jid.c_str()));
    auto_ptr<char> cpass(strdup(pass.c_str()));

    jc = jab_new(cjid.get(), cpass.get());

    jab_packet_handler(jc, &packethandler);
    jab_state_handler(jc, &statehandler);

#if PACKETDEBUG
    jab_logger(jc, &jlogger);
#endif

    jab_start(jc);
    id = atoi(jab_reg(jc));

    if(!online()) {
	err = _("Unable to connect");

    } else {
	regmode = true;
	regdone = false;
	regerr = "";

	while(online() && !regdone && regerr.empty()) {
	    main();
	}

	disconnect();
	err = regdone ? "" : regerr;
    }

    return regdone;
}

void jabberhook::setjabberstatus(imstatus st, const string &msg) {
    xmlnode x = jutil_presnew (JPACKET__UNKNOWN, NULL, NULL);
    xmlnode y = xmlnode_insert_tag (x, "show");

    switch(st) {
	case away: xmlnode_insert_cdata(y, "away", (unsigned) -1); break;
	case dontdisturb: xmlnode_insert_cdata(y, "dnd", (unsigned) -1); break;
	case freeforchat: xmlnode_insert_cdata(y, "chat", (unsigned) -1); break;
	case notavail: xmlnode_insert_cdata(y, "xa", (unsigned) -1); break;
    }

    if(!msg.empty()) {
	y = xmlnode_insert_tag(x, "status");
	xmlnode_insert_cdata(y, msg.c_str(), (unsigned) -1);
    }

    jab_send(jc, x);
    xmlnode_free(x);

    logger.putourstatus(jabber, getstatus(), ourstatus = st);
}

string jabberhook::jidnormalize(const string &jid) {
    string user, host, rest;
    jidsplit(jid, user, host, rest);
    if(host.empty()) host = "jabber.com";
    user += (string) "@" + host;
    if(!rest.empty()) user += (string) "/" + rest;
    return user;
}

string jabberhook::jidtodisp(const string &jid) {
    string user, host, rest;
    jidsplit(jid, user, host, rest);

    if(host != "jabber.com" && !host.empty())
	user += (string) "@" + host;

    return user;
}

void jabberhook::jidsplit(const string &jid, string &user, string &host, string &rest) {
    int pos;
    user = jid;

    if((pos = user.find("/")) != -1) {
	rest = user.substr(pos+1);
	user.erase(pos);
    }

    if((pos = user.find("@")) != -1) {
	host = user.substr(pos+1);
	user.erase(pos);
    }
}

void jabberhook::checkinlist(imcontact ic) {
    icqcontact *c = clist.get(ic);

    if(c)
    if(c->inlist())
    if(find(roster.begin(), roster.end(), jidnormalize(ic.nickname)) != roster.end())
	sendnewuser(ic, false);
}

// ----------------------------------------------------------------------------

void jabberhook::statehandler(jconn conn, int state) {
    static int previous_state = JCONN_STATE_OFF;

    switch(state) {
	case JCONN_STATE_OFF:
	    if(previous_state != JCONN_STATE_OFF) {
		logger.putourstatus(jabber, jhook.getstatus(), jhook.ourstatus = offline);
		face.log(_("+ [jab] disconnected"));
		clist.setoffline(jabber);
		face.update();

		jhook.jc = 0;
		jhook.flogged = false;
		jhook.roster.clear();
	    }
	    break;

	case JCONN_STATE_CONNECTED:
	    break;

	case JCONN_STATE_AUTH:
	    break;

	case JCONN_STATE_ON:
	    jhook.flogged = true;
	    jhook.setjabberstatus(jhook.manualstatus, "");
	    face.log(_("+ [jab] logged in"));
	    face.update();
	    break;

	default:
	    break;
    }

    previous_state = state;
}

void jabberhook::packethandler(jconn conn, jpacket packet) {
    char *p;
    xmlnode x, y;
    string from, type, body, ns;
    imstatus ust;

    jpacket_reset(packet);

    p = xmlnode_get_attrib(packet->x, "from"); if(p) from = p;
    p = xmlnode_get_attrib(packet->x, "type"); if(p) type = p;
    imcontact ic(jidtodisp(from), jabber);

    switch(packet->type) {
	case JPACKET_MESSAGE:
	    x = xmlnode_get_tag(packet->x, "body");
	    p = xmlnode_get_data(x); if(p) body = p;

	    if(x = xmlnode_get_tag(packet->x, "subject")) {
		body = (string) xmlnode_get_data (x) + ": " + body;
	    }

	    if(type == "groupchat") {
	    } else {
		jhook.checkinlist(ic);
		em.store(immessage(ic, imevent::incoming, body));
	    }
	    break;

	case JPACKET_IQ:
	    if(type == "result") {
		if(p = xmlnode_get_attrib(packet->x, "id")) {
		    int iid = atoi(p);

		    if(iid == jhook.id) {
			if(!jhook.regmode) {
			    x = jutil_iqnew (JPACKET__GET, NS_ROSTER);
			    xmlnode_put_attrib(x, "id", "Roster");
			    jab_send(conn, x);
			    xmlnode_free(x);

			    x = jutil_iqnew (JPACKET__GET, NS_AGENTS);
			    xmlnode_put_attrib(x, "id", "Agent List");
			    jab_send(conn, x);
			    xmlnode_free(x);

			} else {
			    jhook.regdone = true;

			}

			return;
		    }
		}

		x = xmlnode_get_tag(packet->x, "query");

		if(x) {
		    p = xmlnode_get_attrib(x, "xmlns"); if(p) ns = p;

		    if(ns == NS_ROSTER) {
			for(y = xmlnode_get_tag(x, "item"); y; y = xmlnode_get_nextsibling(y)) {
			    const char *alias = xmlnode_get_attrib(y, "jid");
			    const char *sub = xmlnode_get_attrib(y, "subscription");
			    const char *name = xmlnode_get_attrib(y, "name");
			    const char *group = xmlnode_get_attrib(y, "group");

			    if(alias) {
				ic = imcontact(jidtodisp(alias), jabber);
				jhook.roster.push_back(jidnormalize(alias));

				if(!clist.get(ic)) {
				    icqcontact *c = clist.addnew(ic, false);
				    if(name) {
					icqcontact::basicinfo cb = c->getbasicinfo();
					cb.fname = name;
					c->setbasicinfo(cb);
				    }
				}
			    }
			}

		    } else if(ns == NS_AGENTS) {
			for(y = xmlnode_get_tag(x, "agent"); y; y = xmlnode_get_nextsibling(y)) {
			    const char *alias = xmlnode_get_attrib(y, "jid");

			    if(alias) {
				const char *name = xmlnode_get_tag_data(y, "name");
				const char *desc = xmlnode_get_tag_data(y, "description");
				const char *service = xmlnode_get_tag_data(y, "service");
				const char *agent_type = 0;
				if(xmlnode_get_tag(y, "groupchat")) agent_type = "groupchat"; else
				if(xmlnode_get_tag(y, "transport")) agent_type = "transport"; else
				if(xmlnode_get_tag(y, "search")) agent_type = "search";
//                                j_add_agent(name, alias, desc, service, from, agent_type);
			    }
			}

		    } else if(ns == NS_AGENT) {
			const char *name = xmlnode_get_tag_data(x, "name");
			const char *url = xmlnode_get_tag_data(x, "url");

		    }

		    jab_send_raw (conn, "<presence/>");
		}

	    } else if(type == "set") {
	    } else if(type == "error") {
		string name, desc;
		int code;

		x = xmlnode_get_tag(packet->x, "error");
		p = xmlnode_get_attrib(x, "code"); if(p) code = atoi(p);
		p = xmlnode_get_attrib(x, "id"); if(p) name = p;
		p = xmlnode_get_tag_data(packet->x, "error"); if(p) desc = p;

		switch(code) {
		    case 401: /* Unauthorized */
		    case 302: /* Redirect */
		    case 400: /* Bad request */
		    case 402: /* Payment Required */
		    case 403: /* Forbidden */
		    case 404: /* Not Found */
		    case 405: /* Not Allowed */
		    case 406: /* Not Acceptable */
		    case 407: /* Registration Required */
		    case 408: /* Request Timeout */
		    case 409: /* Conflict */
		    case 500: /* Internal Server Error */
		    case 501: /* Not Implemented */
		    case 502: /* Remote Server Error */
		    case 503: /* Service Unavailable */
		    case 504: /* Remote Server Timeout */
		    default:
			if(!jhook.regmode) {
			    face.log(_("[jab] error %d: %s"), code, desc.c_str());
			} else {
			    jhook.regerr = desc;
			}
		}

	    }
	    break;

	case JPACKET_PRESENCE:
	    x = xmlnode_get_tag(packet->x, "show");
	    ust = available;

	    if(x) {
		p = xmlnode_get_data(x); if(p) ns = p;

		if(!ns.empty()) {
		    if(ns == "away") ust = away; else
		    if(ns == "dnd") ust = dontdisturb; else
		    if(ns == "xa") ust = notavail; else
		    if(ns == "chat") ust = freeforchat;
		}
	    }

	    if(type == "unavailable")
		ust = offline;

	    if(0/*ischatroom(from)*/) {
		/*JABBERChatRoomBuddyStatus(room, user, status);*/
	    } else {
		icqcontact *c = clist.get(ic);
		if(c) {
		    c->setstatus(ust);

		    if(x = xmlnode_get_tag(packet->x, "status"))
		    if(p = xmlnode_get_data(x))
			jhook.awaymsgs[ic.nickname] = p;
		}
	    }
	    break;

	case JPACKET_S10N:
	    if(type == "subscribe") {
		em.store(imauthorization(ic, imevent::incoming,
		    imauthorization::Request, _("The user wants to subscribe to your network presence updates")));

	    } else if(type == "unsubscribe") {
		auto_ptr<char> cfrom(strdup(from.c_str()));

		x = jutil_presnew(JPACKET__UNSUBSCRIBED, cfrom.get(), 0);
		jab_send(jhook.jc, x);
		xmlnode_free(x);

		em.store(imnotification(ic, (string)
		    _("The user has removed you from his contact list (unsubscribed you, using the Jabber language)")));
	    }
	    break;

	default:
	    break;
    }
}

void jabberhook::jlogger(jconn conn, int inout, const char *p) {
    string tolog = (string) (inout ? "[IN]" : "[OUT]") + "\n";
    tolog += p;
    face.log(tolog);
}
