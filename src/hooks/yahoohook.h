#ifndef __YAHOOHOOK_H__
#define __YAHOOHOOK_H__

#include "abstracthook.h"

#ifdef BUILD_YAHOO

#include "icqconf.h"
#include "pseudoglib.h"

class yahoohook: public abstracthook {
    protected:
	enum Action {
	    tbdConfLogon
	};

	vector<pair<Action, string> > tobedone;

	map<string, Encoding> userenc;
	map<imfile, string> fvalid;
	map<string, string> awaymessages;

	bool fonline, flogged;
	map<string, vector<string> > confmembers;
	imstatus ourstatus;
	int cid;

	time_t timer_refresh;

	static void login_done(guint32 id, int succ, char *url);
	static void got_buddies(guint32 id, struct yahoo_buddy **buds);
	static void status_changed(guint32 id, char *who, int stat, char *msg, int away);
	static void got_im(guint32 id, char *who, char *msg, long tm, int stat);
	static void got_conf_invite(guint32 id, char *who, char *room, char *msg, char **members);
	static void conf_userdecline(guint32 id, char *who, char *room, char *msg);
	static void conf_userjoin(guint32 id, char *who, char *room);
	static void conf_userleave(guint32 id, char *who, char *room);
	static void conf_message(guint32 id, char *who, char *room, char *msg);
	static void got_file(guint32 id, char *who, char *url, long expires, char *msg, char *fname, long fesize);
	static void contact_added(guint32 id, char *myid, char *who, char *msg);
	static void typing_notify(guint32 id, char *who, int stat);
	static void game_notify(guint32 id, char *who, int stat);
	static void mail_notify(guint32 id, char *from, char *subj, int cnt);
	static void system_message(guint32 id, char *msg);
	static void error(guint32 id, char *err, int fatal);
	static void add_input(guint32 id, int fd);
	static void remove_input(guint32 id, int fd);

	static struct tm *timestamp();
	static imstatus yahoo2imstatus(int status);

	string encanalyze(const string &nick, const string &text);

	char **getmembers(const string &room);
	void userstatus(const string &nick, int st, const string &message, bool away);
	void disconnected();

	void checkinlist(imcontact ic);
	void sendnewuser(const imcontact &ic, bool report);
	void removeuser(const imcontact &ic, bool report);

    public:
	yahoohook();
	~yahoohook();

	void init();

	void connect();
	void main();
	void exectimers();
	void disconnect();

	void getsockets(fd_set &rfds, fd_set &wfds, fd_set &efds, int &hsocket) const;
	bool isoursocket(fd_set &rfds, fd_set &wfds, fd_set &efds) const;

	bool online() const;
	bool logged() const;
	bool isconnecting() const;
	bool enabled() const;

	void sendnewuser(const imcontact &ic);
	void removeuser(const imcontact &ic);
	void requestinfo(const imcontact &ic);

	bool send(const imevent &ev);

	void setautostatus(imstatus st);
	imstatus getstatus() const;

	void lookup(const imsearchparams &params, verticalmenu &dest);

	vector<icqcontact *> getneedsync();

	bool knowntransfer(const imfile &fr) const;
	void replytransfer(const imfile &fr, bool accept, const string &localpath = string());
	void aborttransfer(const imfile &fr);

	void requestawaymsg(const imcontact &ic);
	void conferencecreate(const imcontact &confid, const vector<imcontact> &lst);
};

extern yahoohook yhook;

#endif

#endif
