#ifndef __imcontact_H__
#define __imcontact_H__

#include "icqcommon.h"

enum protocolname {
    proto_all = -1,
    icq = 0,
    yahoo,
    msn,
    infocard,

    protocolname_size
};

enum imstatus {
    offline = 0,
    available,
    invisible,
    freeforchat,
    dontdisturb,
    occupied,
    notavail,
    away,
/*
    berightback,
    outtolunch,
    steppedout,
    busy,
    onphone,
    notathome,
    notatdesk,
    notinoffice,
    onvacation,
*/
    imstatus_size
};

static char imstatus2char[imstatus_size] = {
    '_', 'o', 'i', 'f', 'd', 'c', 'n', 'a'/*, 'a',
    'a', 'a', 'c', 'a', 'n', 'n', 'n', 'n'*/
};

struct imcontact {
    imcontact();
    imcontact(unsigned long auin, protocolname apname);
    imcontact(const string &anick, protocolname apname);

    string nickname;
    unsigned long uin;
    protocolname pname;

    bool operator == (const imcontact &ainfo) const;
    bool operator != (const imcontact &ainfo) const;

    bool operator == (protocolname pname) const;
    bool operator != (protocolname pname) const;

    bool empty() const;
    string totext() const;
    string getshortservicename() const;
};

extern imcontact contactroot;

#endif
