/*
*
* centericq contact list class
* $Id: icqcontacts.cc,v 1.10 2001/10/02 17:31:00 konst Exp $
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

#include "icqcontacts.h"
#include "icqmlist.h"
#include "icqconf.h"
#include "icqgroups.h"

icqcontacts::icqcontacts() {
}

icqcontacts::~icqcontacts() {
}

icqcontact *icqcontacts::addnew(unsigned int uin, bool notinlist = true,
bool nonicq = false) {
    icqcontact *c = new icqcontact(uin, nonicq);

    if(!nonicq) {
	if(notinlist) c->excludefromlist();
	c->setnick(i2str(uin));
	c->setdispnick(i2str(uin));
	c->save();
	add(c);
	icq_SendNewUser(&icql, uin);
/*
	icq_ContactAdd(&icql, uin);
	c->setseq2(icq_SendMetaInfoReq(&icql, uin));
*/
    } else {
	c->save();
	add(c);
    }

    return c;
}

void icqcontacts::load() {
    string tname = (string) getenv("HOME") + "/.centericq/", tuname, fname;
    struct dirent *ent;
    struct stat st;
    DIR *d;
    FILE *f;

    empty();

    if(d = opendir(tname.c_str())) {
	while(ent = readdir(d))
	if(strspn(ent->d_name+1, "0123456789") == strlen(ent->d_name)-1) {
	    tuname = tname + ent->d_name;
	    stat(tuname.c_str(), &st);

	    if(S_ISDIR(st.st_mode)) {
		if(ent->d_name[0] == 'n') {
		    clist.add(new icqcontact(atol(ent->d_name+1), true));
		} else if(strchr("0123456789", ent->d_name[0])) {
		    clist.add(new icqcontact(atol(ent->d_name)));
		}
	    }
	}

	closedir(d);
    }

    if(!count) {
	tuname = tname + "17502151";
	mkdir(tuname.c_str(), S_IREAD | S_IWRITE | S_IEXEC);
	icqcontact *c = new icqcontact(17502151);
	add(c);
    }
    
    if(!get(0)) add(new icqcontact(0));
}

void icqcontacts::save() {
    int i;

    for(i = 0; i < count; i++) {
	icqcontact *c = (icqcontact *) at(i);

	if(c->inlist()) c->save(); else
	if(!c->getmsgcount()) {
	    string fname = c->getdirname() + "/offline";
	    if(access(fname.c_str(), F_OK)) c->remove();
	}
    }
}

void icqcontacts::send() {
    int i;
    icqcontact *c;
    icq_ContactClear(&icql);

    for(i = 0; i < count; i++) {
	c = (icqcontact *) at(i);
	
	if(!c->isnonicq())
	if(c->getuin()) {
	    icq_ContactAdd(&icql, c->getuin());
	    icq_ContactSetVis(&icql, c->getuin(),
		lst.inlist(c->getuin(), csvisible) ? ICQ_CONT_VISIBLE :
		lst.inlist(c->getuin(), csinvisible) ? ICQ_CONT_INVISIBLE :
		ICQ_CONT_NORMAL);
	}
    }

    icq_SendContactList(&icql);
    icq_SendVisibleList(&icql);
    icq_SendInvisibleList(&icql);
}

void icqcontacts::remove(unsigned int uin, bool nonicq = false) {
    int i;
    icqcontact *c;

    for(i = 0; i < count; i++) {
	c = (icqcontact *) at(i);

	if(c->isnonicq() == nonicq)
	if(c->getuin() == uin) {
	    c->remove();
	    linkedlist::remove(i);
	    break;
	}
    }
}

icqcontact *icqcontacts::get(unsigned int uin, bool nonicq = false) {
    int i;
    icqcontact *c;

    for(i = 0; i < count; i++) {
	c = (icqcontact *) at(i);

	if(c->isnonicq() == nonicq)
	if(c->getuin() == uin) {
	    return (icqcontact *) at(i);
	}
    }

    return 0;
}

icqcontact *icqcontacts::getseq2(unsigned short seq2) {
    int i;
    icqcontact *c;

    for(i = 0; i < count; i++) {
	c = (icqcontact *) at(i);
	if(c->getseq2() == seq2) {
	    return (icqcontact *) at(i);
	}
    }

    return 0;
}

int icqcontacts::clistsort(void *p1, void *p2) {
    icqcontact *c1 = (icqcontact *) p1, *c2 = (icqcontact *) p2;
    static char *sorder = SORT_CONTACTS;
    char s1, s2;

    s1 = SORTCHAR(c1);
    s2 = SORTCHAR(c2);

    if(!strchr("!N", s1) && !strchr("!N", s2))
    if(!c1->getmsgcount() && !c2->getmsgcount())
    if(conf.getusegroups()) {
	if(c1->getgroupid() > c2->getgroupid()) return -1; else
	if(c1->getgroupid() < c2->getgroupid()) return 1;
    }

    if(s1 == s2) {
	if(*c1 > *c2) return -1; else return 1;
    } else {
	if(strchr(sorder, s1) > strchr(sorder, s2)) return -1; else return 1;
    }
}

void icqcontacts::order() {
    sort(&clistsort);
}

void icqcontacts::rearrange() {
    int i;
    icqcontact *c;

    for(i = 0; i < count; i++) {
	c = (icqcontact *) at(i);
	if(::find(groups.begin(), groups.end(), c->getgroupid()) == groups.end()) {
	    c->setgroupid(1);
	}
    }
}
