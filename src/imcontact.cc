#include "imcontact.h"

imcontact contactroot(0, icq);

imcontact::imcontact() {
}

imcontact::imcontact(unsigned long auin, protocolname apname) {
    uin = auin;
    pname = apname;
}

imcontact::imcontact(const string anick, protocolname apname) {
    nickname = anick;
    pname = apname;
    uin = 0;
}

bool imcontact::operator == (const imcontact &ainfo) const {
    return
	(ainfo.uin == uin) &&
	(ainfo.pname == pname) &&
	(ainfo.nickname == nickname);
}

bool imcontact::operator != (const imcontact &ainfo) const {
    return !(*this == ainfo);
}

bool imcontact::empty() const {
    return (!uin && pname == icq) || (nickname.empty() && pname == yahoo);
}

const string imcontact::totext() const {
    string r;

    switch(pname) {
	case icq:
	    r = "[icq] " + i2str(uin);
	    break;
	case yahoo:
	    r = "[yahoo] " + nickname;
	    break;
	case infocard:
	    r = "[infocard] " + i2str(uin);
	    break;
    }

    return r;
}

const string imcontact::getshortservicename() const {
    string r;

    switch(pname) {
	case icq: r = "I"; break;
	case yahoo: r = "Y"; break;
	case infocard: r = "C"; break;
    }

    return r;
}
