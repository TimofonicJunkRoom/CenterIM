#ifndef __AIMHOOK_H__
#define __AIMHOOK_H__

#include "abstracthook.h"

extern "C" {
#include "firetalk.h"
}

class aimhook: public abstracthook {
    protected:
	struct ourprofile {
	    string info;
	};

	bool fonline, flogged;
	firetalk_t handle;
	imstatus ourstatus;
	ourprofile profile;

	void userstatus(const string &nickname, imstatus st);

	void saveprofile();
	void loadprofile();

	void resolve();

	static void connected(void *conn, void *cli, ...);
	static void disconnected(void *conn, void *cli, ...);
	static void newnick(void *conn, void *cli, ...);
	static void newpass(void *conn, void *cli, ...);
	static void gotinfo(void *conn, void *cli, ...);
	static void getmessage(void *conn, void *cli, ...);
	static void buddyonline(void *conn, void *cli, ...);
	static void buddyoffline(void *conn, void *cli, ...);
	static void buddyaway(void *conn, void *cli, ...);
	static void buddynickchanged(void *conn, void *cli, ...);
	static void needpass(void *conn, void *cli, ...);
	static void connectfailed(void *connection, void *cli, ...);

    public:
	aimhook();
	~aimhook();

	void init();

	void connect();
	void disconnect();
	void exectimers();
	void main();

	void getsockets(fd_set &rfds, fd_set &wfds, fd_set &efds, int &hsocket) const;
	bool isoursocket(fd_set &rfds, fd_set &wfds, fd_set &efds) const;

	bool online() const;
	bool logged() const;
	bool enabled() const;
	bool isconnecting() const;

	bool send(const imevent &ev);

	void sendnewuser(const imcontact &c);
	void removeuser(const imcontact &ic);

	void setautostatus(imstatus st);

	imstatus getstatus() const;

	void requestinfo(const imcontact &c);
	void sendupdateuserinfo(icqcontact &c, const string &newpass);
};

extern aimhook ahook;

#endif