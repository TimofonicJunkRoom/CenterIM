#ifndef __IMEVENTS_H__
#define __IMEVENTS_H__

#include "imcontact.h"

#undef gettext

class imevent {
    friend class immessage;
    friend class imurl;
    friend class imsms;
    friend class imauthorization;
    friend class imemailexpress;
    friend class imemail;

    public:
	enum imeventtype {
	    message, url, sms, authorization, online, email, imeventtype_size
	};

	enum imdirection {
	    incoming, outgoing, imdirection_size
	};

    protected:
	imcontact contact;
	imeventtype type;
	imdirection direction;
	time_t timestamp;

	string readblock(ifstream &f);

    public:
	imevent();
	imevent(const imcontact &acont, imdirection adir, imeventtype atype);
	imevent(ifstream &f);
	virtual ~imevent();

	imeventtype gettype() const;
	imdirection getdirection() const;
	imcontact getcontact() const;
	time_t gettimestamp() const;

	void settimestamp(time_t atimestamp);
	void setcontact(const imcontact &acontact);

	imevent *getevent() const;

	virtual string gettext() const;

	virtual bool empty() const;
	virtual bool contains(const string &atext) const;

	virtual void write(ofstream &f) const;
	virtual void read(ifstream &f);
};

class immessage: public imevent {
    protected:
	string text;

    public:
	immessage(const imevent &ev);
	immessage(const imcontact &acont, imdirection adirection,
	    const string &atext);

	string gettext() const;

	bool empty() const;
	bool contains(const string &atext) const;

	void write(ofstream &f) const;
	void read(ifstream &f);
};

class imurl: public imevent {
    protected:
	string url, description;

    public:
	imurl(const imevent &ev);
	imurl(const imcontact &acont, imdirection adirection,
	    const string &aurl, const string &adescription);

	string geturl() const;
	string getdescription() const;

	string gettext() const;

	bool empty() const;
	bool contains(const string &atext) const;

	void write(ofstream &f) const;
	void read(ifstream &f);
};

class imsms: public imevent {
    protected:
	string text;

    public:
	imsms(const imevent &ev);
	imsms(const imcontact &acont, imdirection adirection,
	    const string &atext);

	string gettext() const;

	bool empty() const;
	bool contains(const string &atext) const;

	void write(ofstream &f) const;
	void read(ifstream &f);
};

class imauthorization: public imevent {
    protected:
	string text;
	bool granted;

    public:
	imauthorization(const imevent &ev);
	imauthorization(const imcontact &acont, imdirection adirection,
	    bool granted, const string &atext);

	string gettext() const;
	bool getgranted() const;

	bool empty() const;
	bool contains(const string &atext) const;

	void write(ofstream &f) const;
	void read(ifstream &f);
};

class imemail: public imevent {
    protected:
	string text;

    public:
	imemail(const imevent &ev);
	imemail(const imcontact &acont, imdirection adirection, const string &atext);

	string gettext() const;

	bool empty() const;
	bool contains(const string &atext) const;

	void write(ofstream &f) const;
	void read(ifstream &f);
};

class imrawevent: public imevent {
    public:
	imrawevent(imeventtype atype, const imcontact &acont, imdirection adirection);
};

#endif
