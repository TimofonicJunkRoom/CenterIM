/*
*
* centericq single icq contact class
* $Id: icqcontact.cc,v 1.29 2001/12/04 17:11:45 konst Exp $
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

#include "icqcontact.h"
#include "icqcontacts.h"
#include "icqgroups.h"
#include "icqconf.h"
#include "centericq.h"
#include "icqface.h"

#include "icqhook.h"
#include "yahoohook.h"

icqcontact::icqcontact(const imcontact adesc) {
    string fname, tname;
    int i;

    clear();
    for(i = 0; i < SOUND_COUNT; i++) sound[i] = "";
    nmsgs = lastread = 0;
    finlist = true;

    cdesc = adesc;

    switch(cdesc.pname) {
	case infocard:
	    if(!cdesc.uin) {
		fname = (string) getenv("HOME") + "/.centericq/n";

		for(i = 1; ; i++) {
		    tname = fname + i2str(i);
		    if(access(tname.c_str(), F_OK)) break;
		}

		cdesc.uin = i;
	    }
	    load();
	    break;

	case icq:
	case yahoo:
	case msn:
	    load();
	    scanhistory();
	    break;
    }
}

icqcontact::~icqcontact() {
}

const string icqcontact::tosane(const string p) const {
    string buf;
    string::iterator i;

    for(buf = p, i = buf.begin(); i != buf.end(); i++) {
	if(strchr("\n\r", *i)) *i = ' ';
    }

    return buf;
}

const string icqcontact::getdirname() const {
    string ret;

    ret = (string) getenv("HOME") + "/.centericq/";

    switch(cdesc.pname) {
	case infocard:
	    ret += "n" + i2str(cdesc.uin);
	    break;
	case icq:
	    ret += i2str(cdesc.uin);
	    break;
	case yahoo:
	    ret += "y" + cdesc.nickname;
	    break;
	case msn:
	    ret += "m" + cdesc.nickname;
	    break;
    }

    return ret;
}

void icqcontact::clear() {
    nmsgs = fupdated = groupid = 0;
    finlist = true;
    status = offline;
    cdesc = contactroot;

    binfo = basicinfo();
    minfo = moreinfo();
    winfo = workinfo();

    nick = dispnick = about = "";
    lastseen = 0;
}

void icqcontact::save() {
    string tname;
    ofstream f;

    mkdir(getdirname().c_str(), S_IREAD | S_IWRITE | S_IEXEC);

    if(!access(getdirname().c_str(), W_OK)) {
	tname = getdirname() + "/lastread";
	f.open(tname.c_str());
	if(f.is_open()) {
	    f << lastread << endl;
	    f.close();
	    f.clear();
	}

	tname = getdirname() + "/info";
	f.open(tname.c_str());
	if(f.is_open()) {
	    f << nick << endl <<
		binfo.fname << endl <<
		binfo.lname << endl <<
		binfo.email << endl <<
		endl <<
		endl <<
		binfo.city << endl <<
		binfo.state << endl <<
		binfo.phone << endl <<
		binfo.fax << endl <<
		binfo.street << endl <<
		binfo.cellular << endl <<
		binfo.zip << endl <<
		binfo.country << endl <<
		winfo.city << endl <<
		winfo.state << endl <<
		winfo.phone << endl <<
		winfo.fax << endl <<
		winfo.street << endl <<
		winfo.zip << endl <<
		winfo.country << endl <<
		winfo.company << endl <<
		winfo.dept << endl <<
		winfo.position << endl <<
		endl <<
		winfo.homepage << endl <<
		(int) minfo.age << endl <<
		(int) minfo.gender << endl <<
		minfo.homepage << endl <<
		endl <<
		endl <<
		endl <<
		minfo.birth_day << endl <<
		minfo.birth_month << endl <<
		minfo.birth_year << endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		lastip << endl <<
		dispnick << endl <<
		lastseen << endl <<
		endl <<
		endl <<
		endl <<
		endl <<
		groupid << endl;

	    f.close();
	    f.clear();
	}

	tname = getdirname() + "/about";
	f.open(tname.c_str());
	if(f.is_open()) {
	    f << about;
	    f.close();
	    f.clear();
	}

	if(!finlist) {
	    tname = getdirname() + "/excluded";
	    f.open(tname.c_str());
	    if(f.is_open()) f.close();
	}
    }
}

void icqcontact::load() {
    int i;
    FILE *f;
    char buf[512];
    struct stat st;
    string tname = getdirname(), fn;

    imcontact savedesc = cdesc;
    clear();
    cdesc = savedesc;

    if(f = fopen((fn = tname + "/info").c_str(), "r")) {
	for(i = 0; !feof(f); i++) {
	    freads(f, buf, 512);
	    switch(i) {
		case  0: nick = buf; break;
		case  1: binfo.fname = buf; break;
		case  2: binfo.lname = buf; break;
		case  3: binfo.email = buf; break;
		case  4: break;
		case  5: break;
		case  6: binfo.city = buf; break;
		case  7: binfo.state = buf; break;
		case  8: binfo.phone = buf; break;
		case  9: binfo.fax = buf; break;
		case 10: binfo.street = buf; break;
		case 11: binfo.cellular = buf; break;
		case 12: binfo.zip = strtoul(buf, 0, 0); break;
		case 13: binfo.country = buf; break;
		case 14: winfo.city = buf; break;
		case 15: winfo.state = buf; break;
		case 16: winfo.phone = buf; break;
		case 17: winfo.fax = buf; break;
		case 18: winfo.street = buf; break;
		case 19: winfo.zip = strtoul(buf, 0, 0); break;
		case 20: winfo.country = buf; break;
		case 21: winfo.company = buf; break;
		case 22: winfo.dept = buf; break;
		case 23: winfo.position = buf; break;
		case 24: break;
		case 25: winfo.homepage = buf; break;
		case 26: minfo.age = atoi(buf); break;
		case 27: minfo.gender = (imgender) atoi(buf); break;
		case 28: minfo.homepage = buf; break;
		case 29: break;
		case 30: break;
		case 31: break;
		case 32: minfo.birth_day = atoi(buf); break;
		case 33: minfo.birth_month = atoi(buf); break;
		case 34: minfo.birth_year = atoi(buf); break;
		case 35: break;
		case 36: break;
		case 37: break;
		case 38: break;
		case 39: break;
		case 40: break;
		case 41: break;
		case 42: break;
		case 43: break;
		case 44: lastip = buf; break;
		case 45: dispnick = buf; break;
		case 46: lastseen = strtoul(buf, 0, 0); break;
		case 47: break;
		case 48: break;
		case 49: break;
		case 50: break;
		case 51: groupid = atoi(buf); break;
	    }
	}
	fclose(f);
    }

    if(f = fopen((fn = tname + "/about").c_str(), "r")) {
	while(!feof(f)) {
	    freads(f, buf, 512);
	    if(about.size()) about += '\n';
	    about += buf;
	}
	fclose(f);
    }

    if(f = fopen((fn = tname + "/lastread").c_str(), "r")) {
	freads(f, buf, 512);
	sscanf(buf, "%lu", &lastread);
	fclose(f);
    }

    finlist = stat((fn = tname + "/excluded").c_str(), &st);

    if(nick.empty()) nick = i2str(cdesc.uin);
    if(dispnick.empty()) dispnick = nick;

    if(conf.getusegroups())
    if(find(groups.begin(), groups.end(), groupid) == groups.end()) {
	groupid = 1;
    }
}

bool icqcontact::isbirthday() const {
    bool ret = false;
    time_t curtime = time(0);
    struct tm tbd, *tcur = localtime(&curtime);

    memset(&tbd, 0, sizeof(tbd));

    tbd.tm_year = tcur->tm_year;
    tbd.tm_mday = minfo.birth_day;
    tbd.tm_mon = minfo.birth_month-1;

    if(tbd.tm_mday == tcur->tm_mday)
    if(tbd.tm_mon == tcur->tm_mon) {
	ret = true;
    }

    return ret;
}

void icqcontact::remove() {
    string dname = getdirname(), fname;
    struct dirent *e;
    struct stat st;
    DIR *d;

    gethook(cdesc.pname).removeuser(cdesc);

    if(d = opendir(dname.c_str())) {
	while(e = readdir(d)) {
	    fname = dname + "/" + e->d_name;
	    if(!stat(fname.c_str(), &st) && !S_ISDIR(st.st_mode))
		unlink(fname.c_str());
	}
	closedir(d);
	rmdir(dname.c_str());
    }
}

void icqcontact::excludefromlist() {
    FILE *f;
    string fname = getdirname() + "/excluded";
    if(f = fopen(fname.c_str(), "w")) fclose(f);
    finlist = false;
}

void icqcontact::includeintolist() {
    unlink((getdirname() + "/excluded").c_str());
    finlist = true;
}

bool icqcontact::inlist() const {
    return finlist;
}

void icqcontact::scanhistory() {
    string fn = getdirname() + "/history";
    char buf[512];
    int line;
    FILE *f = fopen(fn.c_str(), "r");
    bool docount;

    nmsgs = 0;
    if(f) {
	while(!feof(f)) {
	    freads(f, buf, 512);

	    if((string) buf == "\f") line = 0; else
	    switch(line) {
		case 1: docount = ((string) buf == "IN"); break;
		case 4: if(docount && (atol(buf) > lastread)) nmsgs++; break;
	    }

	    line++;
	}
	fclose(f);
    }
}

void icqcontact::setstatus(imstatus fstatus) {
    if(status != fstatus) {
	status = fstatus;
	face.relaxedupdate();
    }
}

void icqcontact::setnick(const string fnick) {
    nick = fnick;
}

void icqcontact::setdispnick(const string fnick) {
    dispnick = fnick;
}

void icqcontact::setlastread(time_t flastread) {
    lastread = flastread;
    scanhistory();
}

void icqcontact::unsetupdated() {
    fupdated = 0;
}

void icqcontact::setmsgcount(int n) {
    nmsgs = n;
    face.relaxedupdate();
}

#define recode(f) binfo.f = rusconv("wk", binfo.f)

void icqcontact::setbasicinfo(const basicinfo &ainfo) {
    binfo = ainfo;
    fupdated++;

    recode(fname);
    recode(lname);
    recode(email);
    recode(city);
    recode(state);
    recode(phone);
    recode(fax);
    recode(street);
    recode(cellular);
    recode(country);
}

#undef recode
#define recode(f) minfo.f = rusconv("wk", minfo.f)

void icqcontact::setmoreinfo(const moreinfo &ainfo) {
    minfo = ainfo;
    fupdated++;

    recode(homepage);
}

#undef recode
#define recode(f) winfo.f = rusconv("wk", winfo.f)

void icqcontact::setworkinfo(const workinfo &ainfo) {
    winfo = ainfo;
    fupdated++;

    recode(city);
    recode(state);
    recode(phone);
    recode(fax);
    recode(street);
    recode(country);
    recode(company);
    recode(dept);
    recode(position);
    recode(homepage);
}

const icqcontact::basicinfo &icqcontact::getbasicinfo() const {
    return binfo;
}

const icqcontact::moreinfo &icqcontact::getmoreinfo() const {
    return minfo;
}

const icqcontact::workinfo &icqcontact::getworkinfo() const {
    return winfo;
}

void icqcontact::setabout(const string data) {
    about = rusconv("wk", data);
    fupdated++;
}

void icqcontact::setsound(int event, const string sf) {
    if(event <= SOUND_COUNT) {
	sound[event] = sf;
    }
}

void icqcontact::playsound(int event) const {
    string sf = sound[event], cline;
    int i;

    if(event <= SOUND_COUNT) {
	if(sf.size()) {
	    if(sf[0] == '!') {
		time_t lastmelody = 0;

		if(time(0)-lastmelody < 5) return;
		time(&lastmelody);

		if(sf.substr(1) == "spk1") {
		    for(i = 0; i < 3; i++) {
			if(i) delay(90);
			setbeep((i+1)*100, 60);
			printf("\a");
			fflush(stdout);
		    }
		} else if(sf.substr(1) == "spk2") {
		    for(i = 0; i < 2; i++) {
			if(i) delay(90);
			setbeep((i+1)*300, 60);
			printf("\a");
			fflush(stdout);
		    }
		} else if(sf.substr(1) == "spk3") {
		    for(i = 3; i > 0; i--) {
			setbeep((i+1)*200, 60-i*10);
			printf("\a");
			fflush(stdout);
			delay(90-i*10);
		    }
		} else if(sf.substr(1) == "spk4") {
		    for(i = 0; i < 4; i++) {
			setbeep((i+1)*400, 60);
			printf("\a");
			fflush(stdout);
			delay(90);
		    }
		} else if(sf.substr(1) == "spk5") {
		    for(i = 0; i < 4; i++) {
			setbeep((i+1)*250, 60+i);
			printf("\a");
			fflush(stdout);
			delay(90-i*5);
		    }
		}
	    } else {
		static int pid = 0;

		if(pid) kill(pid, SIGKILL);
		pid = fork();
		if(!pid) {
		    string cline = sf + " >/dev/null 2>&1";
		    execlp("/bin/sh", "/bin/sh", "-c", cline.c_str(), 0);
		    exit(0);
		}
	    }
	} else if(cdesc != contactroot) {
	    icqcontact *c = clist.get(contactroot);
	    c->playsound(event);
	}
    }
}

void icqcontact::setlastip(const string flastip) {
    lastip = flastip;
}

const string icqcontact::getabout() const {
    return about;
}

const string icqcontact::getlastip() const {
    return lastip;
}

time_t icqcontact::getlastread() const {
    return lastread;
}

imstatus icqcontact::getstatus() const {
    return status;
}

int icqcontact::getmsgcount() const {
    return nmsgs;
}

const string icqcontact::getnick() const {
    return nick;
}

const string icqcontact::getdispnick() const {
    return dispnick;
}

int icqcontact::updated() const {
    return fupdated;
}

void icqcontact::setlastseen() {
    time(&lastseen);
}

time_t icqcontact::getlastseen() const {
    return lastseen;
}

char icqcontact::getshortstatus() const {
    return imstatus2char[status];
}

bool icqcontact::operator > (const icqcontact &acontact) const {
    if(acontact.lastread != lastread) {
	return acontact.lastread > lastread;
    } else if(acontact.cdesc.uin != cdesc.uin) {
	return acontact.cdesc.uin > cdesc.uin;
    } else {
	return acontact.cdesc.nickname.compare(cdesc.nickname);
    }
}

void icqcontact::setpostponed(const string apostponed) {
    postponed = apostponed;
}

const string icqcontact::getpostponed() const {
    return postponed;
}

void icqcontact::setgroupid(int agroupid) {
    groupid = agroupid;
}

int icqcontact::getgroupid() const {
    return groupid;
}

const imcontact icqcontact::getdesc() const {
    return cdesc;
}

// ----------------------------------------------------------------------------

const string icqcontact::moreinfo::strbirthdate() const {
    string r;

    static const string smonths[12] = {
	_("Jan"), _("Feb"), _("Mar"), _("Apr"),
	_("May"), _("Jun"), _("Jul"), _("Aug"),
	_("Sep"), _("Oct"), _("Nov"), _("Dec")
    };

    if((birth_day > 0) && (birth_day <= 31))
    if((birth_month > 0) && (birth_month <= 12)) {
	r = i2str(birth_day) + "-" + smonths[birth_month-1] + "-" + i2str(birth_year);
    }

    return r;
}
