#ifndef __YAHOOHOOK_H__
#define __YAHOOHOOK_H__

#include "icqconf.h"
#include "abstracthook.h"

class yahoohook: public abstracthook {
    protected:
	struct yahoo_context *context;
	bool fonline;
	int ourstatus;

	time_t timer_reconnect;

	static void disconnected(yahoo_context *y);
	static void userlogon(yahoo_context *y, const char *nick, int status);
	static void userlogoff(yahoo_context *y, const char *nick);
	static void userstatus(yahoo_context *y, const char *nick, int status);
	static void recvbounced(yahoo_context *y, const char *nick);
	static void recvmessage(yahoo_context *y, const char *nick, const char *msg);
	static void log(yahoo_context *y, const char *msg);

	static struct tm *timestamp();

	imstatus yahoo2imstatus(int status) const;
	void initcontext(const icqconf::imaccount account);

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
	bool enabled() const;

	void sendnewuser(const imcontact ic);
	void removeuser(const imcontact ic);

	bool send(const imevent &ev);

	void setautostatus(imstatus st);
	imstatus getstatus() const;
};

extern yahoohook yhook;

#endif
