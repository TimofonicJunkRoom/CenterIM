/*
*
* centericq main() function
* $Id: centermain.cc,v 1.13 2001/12/05 17:13:46 konst Exp $
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

#include "centericq.h"
#include "icqhook.h"
#include "yahoohook.h"
#include "icqface.h"
#include "icqconf.h"
#include "icqcontacts.h"
#include "icqmlist.h"
#include "icqgroups.h"

centericq cicq;
icqconf conf;
icqcontacts clist;
icqface face;
icqlist lst;
icqgroups groups;

#ifdef ENABLE_NLS

//#include <locale.h>
#include <libintl.h>

#endif

int main(int argc, char **argv) {
    char savedir[1024];

    getcwd(savedir, 1024);

    try {

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALE_DIR);
	textdomain(PACKAGE);
#endif

	conf.commandline(argc, argv);
	cicq.exec();

    } catch(int errcode) {
    }

    chdir(savedir);
}
