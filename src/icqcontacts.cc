#include "icqcontacts.h"
#include "icqmlist.h"
#include "icqconf.h"

icqcontacts::icqcontacts() {
    visuins.freeitem = &nothingfree;
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
	c->setseq2(icq_SendMetaInfoReq(&icql, uin));
	icq_SendNewUser(&icql, uin);
	icq_ContactAdd(&icql, uin);
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
	    if(lst.inlist(c->getuin(), csvisible)) {
		icq_ContactSetVis(&icql, c->getuin(), 1);
	    }
	}
    }

    icq_SendContactList(&icql);
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
    int i;
    icqcontact *c1 = (icqcontact *) p1, *c2 = (icqcontact *) p2;
    char s1 = c1->getsortchar(), s2 = c2->getsortchar();
    static char *sorder = SORT_CONTACTS;

    if(s1 == 'O') s1 = c1->getshortstatus();
    if(s2 == 'O') s2 = c2->getshortstatus();

    if(c1->getmsgcount()) s1 = '#';
    if(c2->getmsgcount()) s2 = '#';

    if(s1 == s2) {
	if(*c1 > *c2) return -1; else return 1;
    } else {
	if(strchr(sorder, s1) > strchr(sorder, s2)) return -1; else return 1;
    }
}

void icqcontacts::order() {
    sort(&clistsort);
}
