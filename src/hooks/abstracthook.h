#ifndef __ABSTRACTHOOK_H__
#define __ABSTRACTHOOK_H__

#include "icqcontact.h"
#include "imevents.h"

enum hookcapabilities {
    hoptCanNotify = 2,
    hoptCanSendURL = 4,
    hoptCanSendFile = 8,
    hoptCanSendSMS = 16
};

class abstracthook {
    protected:
	imstatus manualstatus;
	int fcapabilities;

    public:
	abstracthook();

	virtual void init();

	virtual void connect();
	virtual void disconnect();
	virtual void exectimers();
	virtual void main();

	virtual void getsockets(fd_set &fds, int &hsocket) const;
	virtual bool isoursocket(fd_set &fds) const;

	virtual bool online() const;
	virtual bool logged() const;
	virtual bool isconnecting() const;
	virtual bool enabled() const;

	virtual bool send(const imevent &ev);

	virtual void sendnewuser(const imcontact c);
	virtual void removeuser(const imcontact ic);

	virtual void setautostatus(imstatus st);
	virtual void restorestatus();

	virtual void setstatus(imstatus st);
	virtual imstatus getstatus() const;

	virtual bool isdirectopen(const imcontact c) const;
	virtual void requestinfo(const imcontact c);

	int getcapabilities() const;
};

abstracthook &gethook(protocolname pname);
struct tm *maketm(int hour, int minute, int day, int month, int year);

#endif
