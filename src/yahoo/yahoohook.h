#ifndef __YAHOOHOOK_H__
#define __YAHOOHOOK_H__

#include "icqconf.h"

class yahoohook {
    protected:
	struct yahoo_context *yahoo;
	bool fonline;
	int ourstatus;
	imstatus manualstatus;

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
	void init(const icqconf::imaccount account);

    public:
	yahoohook();
	~yahoohook();

	void connect();
	void main();
	void exectimers();
	void disconnect();

	int getsockfd() const;
	bool online() const;
	bool logged() const;
	bool enabled() const;

	void sendnewuser(const imcontact ic);
	void removeuser(const imcontact ic);

	unsigned long sendmessage(const icqcontact *c, const string text);
	void setstatus(imstatus st);
	imstatus getstatus() const;
};

extern yahoohook yhook;

#endif