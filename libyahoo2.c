/*
 * libyahoo2: libyahoo2.c
 *
 * Some code copyright (C) 2002, Philip S Tellis <philip.tellis@iname.com>
 *
 * Much of this code was taken and adapted from the yahoo module for
 * gaim released under the GNU GPL.  This code is also released under the 
 * GNU GPL.
 *
 * This code is derivitive of Gaim <http://gaim.sourceforge.net>
 * copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *             1998-1999, Adam Fritzler <afritz@marko.net>
 *             1998-2002, Rob Flynn <rob@marko.net>
 *             2000-2002, Eric Warmenhoven <eric@warmenhoven.org>
 *             2001-2002, Brian Macke <macke@strangelove.net>
 *                  2001, Anand Biligiri S <abiligiri@users.sf.net>
 *                  2001, Valdis Kletnieks
 *                  2002, Sean Egan <bj91704@binghamton.edu>
 *                  2002, Toby Gray <toby.gray@ntlworld.com>
 *
 * This library also uses code from other libraries, namely:
 *     Portions from libfaim copyright 1998, 1999 Adam Fritzler
 *     <afritz@auk.cx>
 *     Portions of Sylpheed copyright 2000-2002 Hiroyuki Yamamoto
 *     <hiro-y@kcn.ne.jp>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <glib.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include "md5.h"
#include "libyahoo2.h"
#include "yahoo_connections.h"
#include "yahoo_httplib.h"

#include "yahoo2_callbacks.h"


extern char pager_host[];
extern char pager_port[];
extern char filetransfer_host[];
extern char filetransfer_port[];

enum yahoo_service { /* these are easier to see in hex */
	YAHOO_SERVICE_LOGON = 1,
	YAHOO_SERVICE_LOGOFF,
	YAHOO_SERVICE_ISAWAY,
	YAHOO_SERVICE_ISBACK,
	YAHOO_SERVICE_IDLE, /* 5 (placemarker) */
	YAHOO_SERVICE_MESSAGE,
	YAHOO_SERVICE_IDACT,
	YAHOO_SERVICE_IDDEACT,
	YAHOO_SERVICE_MAILSTAT,
	YAHOO_SERVICE_USERSTAT, /* 0xa */
	YAHOO_SERVICE_NEWMAIL,
	YAHOO_SERVICE_CHATINVITE,
	YAHOO_SERVICE_CALENDAR,
	YAHOO_SERVICE_NEWPERSONALMAIL,
	YAHOO_SERVICE_NEWCONTACT,
	YAHOO_SERVICE_ADDIDENT, /* 0x10 */
	YAHOO_SERVICE_ADDIGNORE,
	YAHOO_SERVICE_PING,
	YAHOO_SERVICE_GROUPRENAME,
	YAHOO_SERVICE_SYSMESSAGE = 0x14,
	YAHOO_SERVICE_PASSTHROUGH2 = 0x16,
	YAHOO_SERVICE_CONFINVITE = 0x18,
	YAHOO_SERVICE_CONFLOGON,
	YAHOO_SERVICE_CONFDECLINE,
	YAHOO_SERVICE_CONFLOGOFF,
	YAHOO_SERVICE_CONFADDINVITE,
	YAHOO_SERVICE_CONFMSG,
	YAHOO_SERVICE_CHATLOGON,
	YAHOO_SERVICE_CHATLOGOFF,
	YAHOO_SERVICE_CHATMSG = 0x20,
	YAHOO_SERVICE_GAMELOGON = 0x28,
	YAHOO_SERVICE_GAMELOGOFF,
	YAHOO_SERVICE_GAMEMSG = 0x2a,
	YAHOO_SERVICE_FILETRANSFER = 0x46,
	YAHOO_SERVICE_VOICECHAT = 0x4A,
	YAHOO_SERVICE_NOTIFY = 0x4B,
	YAHOO_SERVICE_P2PFILEXFER = 0x4D,
	YAHOO_SERVICE_UNKNOWN2 = 0x4F,  /* PEERTOPEER */
	YAHOO_SERVICE_AUTHRESP = 0x54,
	YAHOO_SERVICE_LIST = 0x55,
	YAHOO_SERVICE_AUTH = 0x57,
	YAHOO_SERVICE_ADDBUDDY = 0x83,
	YAHOO_SERVICE_REMBUDDY = 0x84
};

struct yahoo_pair {
	int key;
	char *value;
};

struct yahoo_packet {
	guint16 service;
	guint32 status;
	guint32 id;
	GSList *hash;
};

static struct yahoo_connections *conn_list;
static struct yahoo_callbacks *ycb=NULL;
static guint32 last_id=0;

extern char *yahoo_crypt(char *, char *);

//#define DEBUG 1

#undef DEBUG

#ifdef DEBUG
#define debug_printf printf
#else
static void debug_printf(char *fmt, ...)
{
}
#endif

/* Free a buddy list */
static void yahoo_free_buddies(struct yahoo_data *yd)
{
	int i;

	if (!yd->buddies)
	{
		return;
	}

	i = 0;
	while (yd->buddies[i])
	{
		g_free(yd->buddies[i]->group);
		g_free(yd->buddies[i]->id);
		g_free(yd->buddies[i]->real_name);
		g_free(yd->buddies[i]);
		i++;
	}

	g_free(yd->buddies);
}

/* Free a identities list */
static void yahoo_free_identities(struct yahoo_data *yd)
{
	int i;

	if (!yd->identities)
	{
		return;
	}

	i = 0;
	while (yd->identities[i])
	{
		g_free(yd->identities[i]);
		i++;
	}

	g_free(yd->identities);
}

static void yahoo_free_data(struct yahoo_data *yd)
{
	g_free(yd->user);
	g_free(yd->password);
	g_free(yd->cookie_y);
	g_free(yd->cookie_t);
	g_free(yd->login_cookie);
	g_free(yd->login_id);

	yahoo_free_buddies(yd);
	yahoo_free_identities(yd);

	g_free(yd);
}

static gboolean yahoo_destroy_hash(gpointer key, gpointer val, gpointer data)
{
	g_free(key);
	g_free(val);
	return TRUE;
}

#define YAHOO_PACKET_HDRLEN (4 + 2 + 2 + 2 + 2 + 4 + 4)

static struct yahoo_packet *yahoo_packet_new(enum yahoo_service service, 
		enum yahoo_status status, int id)
{
	struct yahoo_packet *pkt = g_new0(struct yahoo_packet, 1);

	pkt->service = service;
	pkt->status = status;
	pkt->id = id;

	return pkt;
}

static void yahoo_packet_hash(struct yahoo_packet *pkt, int key, char *value)
{
	struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);
	pair->key = key;
	pair->value = g_strdup(value);
	pkt->hash = g_slist_append(pkt->hash, pair);
}

static int yahoo_packet_length(struct yahoo_packet *pkt)
{
	GSList *l;

	int len = 0;

	l = pkt->hash;
	while (l) {
		struct yahoo_pair *pair = l->data;
		int tmp = pair->key;
		do {
			tmp /= 10;
			len++;
		} while (tmp);
		len += 2;
		len += strlen(pair->value);
		len += 2;
		l = l->next;
	}

	return len;
}

#define yahoo_put16(buf, data) ( \
		(*(buf) = (u_char)((data)>>8)&0xff), \
		(*((buf)+1) = (u_char)(data)&0xff),  \
		2)
#define yahoo_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define yahoo_put32(buf, data) ( \
		(*((buf)) = (u_char)((data)>>24)&0xff), \
		(*((buf)+1) = (u_char)((data)>>16)&0xff), \
		(*((buf)+2) = (u_char)((data)>>8)&0xff), \
		(*((buf)+3) = (u_char)(data)&0xff), \
		4)
#define yahoo_get32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3)    )&0x000000ff)))

int yahoo_connect(char *host, int port)
{
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int servfd;
	int res;
	char **p;

	if(!(server = gethostbyname(host)))
	{
//                fprintf(stderr, "failed to look up server (%s:%d)\n", host, port);
		return -1;
	}

	if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -2;

//        fprintf(stderr, "Created a local socket\n");

//        fprintf(stderr, "connecting to %s:%d\n", host, port);
	for (p = server->h_addr_list; *p; p++)
	{
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		memcpy(&serv_addr.sin_addr.s_addr, *p, server->h_length);
		serv_addr.sin_port = htons(port);

		res = -1;
		res = connect(servfd, (struct sockaddr *) &serv_addr, 
				sizeof(serv_addr));

		if(res >= 0)
			return servfd;
	}

//        fprintf(stderr, "Could not connect to %s:%d\n", host, port);
	close(servfd);
	return -3;
}

static void yahoo_packet_read(struct yahoo_packet *pkt, guchar *data, int len)
{
	int pos = 0;

	while (pos + 1 < len) {
		char key[64], *value = NULL;
		int accept;
		int x;

		struct yahoo_pair *pair = g_new0(struct yahoo_pair, 1);

		x = 0;
		while (pos + 1 < len) {
			if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
				break;
			key[x++] = data[pos++];
		}
		key[x] = 0;
		pos += 2;
		pair->key = strtol(key, NULL, 10);
		accept = x; 
		/* if x is 0 there was no key, so don't accept it */
		if (accept)
			value = g_malloc(len - pos + 1);
		x = 0;
		while (pos + 1 < len) {
			if (data[pos] == 0xc0 && data[pos + 1] == 0x80)
				break;
			if (accept)
				value[x++] = data[pos++];
		}
		if (accept)
			value[x] = 0;
		pos += 2;
		if (accept) {
			pair->value = g_strdup(value);
			g_free(value);
			pkt->hash = g_slist_append(pkt->hash, pair);
			debug_printf("Key: %d  \tValue: %s\n", 
					pair->key, pair->value);
		} else {
			g_free(pair);
		}
	}
}

static void yahoo_packet_write(struct yahoo_packet *pkt, guchar *data)
{
	GSList *l = pkt->hash;
	int pos = 0;

	while (l) {
		struct yahoo_pair *pair = l->data;
		guchar buf[100];

		g_snprintf(buf, sizeof(buf), "%d", pair->key);
		strcpy(data + pos, buf);
		pos += strlen(buf);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		strcpy(data + pos, pair->value);
		pos += strlen(pair->value);
		data[pos++] = 0xc0;
		data[pos++] = 0x80;

		l = l->next;
	}
}

static void yahoo_dump_unhandled(struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

//        fprintf(stderr, "Service: 0x%02x\tStatus: %d\n", pkt->service, pkt->status);
	while (l) {
		struct yahoo_pair *pair = l->data;
//                fprintf(stderr, "\t%d => %s\n", pair->key, pair->value);
		l = l->next;
	}
}


static void yahoo_packet_dump(guchar *data, int len)
{
#ifdef DEBUG
	int i;
	for (i = 0; i < len; i++) {
		if ((i % 8 == 0) && i)
			debug_printf(" ");
		if ((i % 16 == 0) && i)
			debug_printf("\n");
		debug_printf("%02x ", data[i]);
	}
	debug_printf("\n");
	for (i = 0; i < len; i++) {
		if ((i % 8 == 0) && i)
			debug_printf(" ");
		if ((i % 16 == 0) && i)
			debug_printf("\n");
		if (isprint(data[i]))
			debug_printf(" %c ", data[i]);
		else
			debug_printf(" . ");
	}
	debug_printf("\n");
#endif
}

static char base64digits[] =    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz"
				"0123456789._";
static void to_y64(unsigned char *out, const unsigned char *in, int inlen)
/* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */
{
	for (; inlen >= 3; inlen -= 3)
		{
			*out++ = base64digits[in[0] >> 2];
			*out++ = base64digits[((in[0]<<4) & 0x30) | (in[1]>>4)];
			*out++ = base64digits[((in[1]<<2) & 0x3c) | (in[2]>>6)];
			*out++ = base64digits[in[2] & 0x3f];
			in += 3;
		}
	if (inlen > 0)
		{
			unsigned char fragment;

			*out++ = base64digits[in[0] >> 2];
			fragment = (in[0] << 4) & 0x30;
			if (inlen > 1)
				fragment |= in[1] >> 4;
			*out++ = base64digits[fragment];
			*out++ = (inlen < 2) ? '-' 
					: base64digits[(in[1] << 2) & 0x3c];
			*out++ = '-';
		}
	*out = '\0';
}

static int yahoo_send_packet(struct yahoo_data *yd, struct yahoo_packet *pkt, int extra_pad)
{
	int pktlen = yahoo_packet_length(pkt);
	int len = YAHOO_PACKET_HDRLEN + pktlen;
	int ret;

	guchar *data;
	int pos = 0;

	if (yd->fd < 0)
		return -1;

	data = g_malloc0(len + 1);

	memcpy(data + pos, "YMSG", 4); pos += 4;
	pos += yahoo_put16(data + pos, 0x0900);
	pos += yahoo_put16(data + pos, 0x0000);
	pos += yahoo_put16(data + pos, pktlen + extra_pad);
	pos += yahoo_put16(data + pos, pkt->service);
	pos += yahoo_put32(data + pos, pkt->status);
	pos += yahoo_put32(data + pos, pkt->id);

	yahoo_packet_write(pkt, data + pos);

	yahoo_packet_dump(data, len);
	ret = write(yd->fd, data, len);
	debug_printf("wrote packet\n");

	g_free(data);

	return ret;
}

static void yahoo_packet_free(struct yahoo_packet *pkt)
{
	while (pkt->hash) {
		struct yahoo_pair *pair = pkt->hash->data;
		g_free(pair->value);
		g_free(pair);
		pkt->hash = g_slist_remove(pkt->hash, pair);
	}
	g_free(pkt);
}

static struct yahoo_buddy **getbuddylist(char *rawlist)
{
	struct yahoo_buddy **buddylist=NULL;
	int buddycnt=0;
	int nextbuddy=0;

	char **lines;
	char **split;
	char **buddies;
	char **tmp, **bud;

	lines = g_strsplit(rawlist, "\n", -1);
	for (tmp = lines; *tmp; tmp++) {
		int tmp_buddycnt;
		struct yahoo_buddy **tmp_buddylist;
		struct yahoo_buddy *tmpbuddy;

		split = g_strsplit(*tmp, ":", 2);
		if (!split)
			continue;
		if (!split[0] || !split[1]) {
			g_strfreev(split);
			continue;
		}
		buddies = g_strsplit(split[1], ",", -1);
		/* count number of buddies in this list */
		tmp_buddycnt = buddycnt;
		for (bud = buddies; bud && *bud; bud++)
			buddycnt++;
		buddycnt++;     /* one more */

		/* allocate the array to hold the list of buddy */
		tmp_buddylist = (struct yahoo_buddy **)g_malloc0(
			sizeof(struct yahoo_buddy *) * (buddycnt + 1));

		/* Free and copy the old one if necessary */
		if (buddylist)
		{
			memcpy(tmp_buddylist, buddylist,
				(tmp_buddycnt + 1) * 
				sizeof(struct yahoo_buddy *));

			g_free(buddylist);
		}
		buddylist = tmp_buddylist;

		for (bud = buddies; bud && *bud; bud++) {
			int tmpid = 0;
			for(; buddylist[tmpid]; tmpid++)
				if(!strcmp(buddylist[tmpid]->id, *bud))
					goto next;

			tmpbuddy = g_new0(struct yahoo_buddy, 1);
			tmpbuddy->id = strdup(*bud);
			tmpbuddy->group = strdup(split[0]);
			tmpbuddy->real_name = NULL;
			buddylist[nextbuddy++] = tmpbuddy;
			buddylist[nextbuddy] = NULL;

//                        fprintf(stderr, "Added buddy %s to group %s\n",
//                                        tmpbuddy->id, tmpbuddy->group);
		next:
		}

		g_strfreev(buddies);
		g_strfreev(split);
	}
	g_strfreev(lines);

	return buddylist;
}

static char * getcookie(char *rawcookie)
{
	char * cookie=NULL;
	char * tmpcookie = strdup(rawcookie+2);
	char * cookieend = strchr(tmpcookie, ';');

	if(cookieend)
		*cookieend = '\0';

	cookie = strdup(tmpcookie);
	g_free(tmpcookie);
	cookieend=NULL;

	return cookie;
}

static char * getlcookie(char *cookie)
{
	char *tmp;
	char *tmpend;
	char *login_cookie = NULL;

	tmpend = strstr(cookie, "n=");
	if(tmpend) {
		tmp = strdup(tmpend+2);
		tmpend = strchr(tmp, '&');
		if(tmpend)
			*tmpend='\0';
		login_cookie = strdup(tmp);
		g_free(tmp);
	}

	return login_cookie;
}

static void yahoo_process_notify(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	char *stat = NULL;
	char *game = NULL;
	GSList *l = pkt->hash;
	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4)
			from = pair->value;
		if (pair->key == 49)
			msg = pair->value;
		if (pair->key == 13)
			stat = pair->value;
		if (pair->key == 14)
			game = pair->value;
		l = l->next;
	}
	
	if (!g_strncasecmp(msg, "TYPING", strlen("TYPING"))) {
		int typ=0;
		if (*stat == '1')
			typ = 1;
		if(ycb->yahoo_typing_notify)
			ycb->yahoo_typing_notify(yd->client_id, from, typ);
	} else if (!g_strncasecmp(msg, "GAME", strlen("GAME"))) {
		void *free1=NULL, *free2=NULL;
		if (*stat == '1') {
			if (g_hash_table_lookup_extended (yd->games, from, free1, free2)) {
				g_free(free1);
				g_free(free2);
			}
			g_hash_table_insert (yd->games, g_strdup(from), g_strdup(game));
			if(ycb->yahoo_game_notify)
				ycb->yahoo_game_notify(yd->client_id, from, 1);
		} else {
			if (g_hash_table_lookup_extended (yd->games, from, free1, free2)) {
				g_free(free1);
				g_free(free2);
				g_hash_table_remove (yd->games, from);
			}
			if(ycb->yahoo_game_notify)
				ycb->yahoo_game_notify(yd->client_id, from, 0);
		}
	}
}

static void yahoo_process_filetransfer(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *from=NULL;
	char *to=NULL;
	char *msg=NULL;
	char *url=NULL;
	long expires=0;

	char *service=NULL;

	char *filename=NULL;
	long filesize=0L;

	GSList *l = pkt->hash;
	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4)
			from = pair->value;
		if (pair->key == 5)
			to = pair->value;
		if (pair->key == 14)
			msg = pair->value;
		if (pair->key == 20)
			url = pair->value;
		if (pair->key == 38)
			expires = atol(pair->value);

		if (pair->key == 27)
			filename = pair->value;
		if (pair->key == 28)
			filesize = atol(pair->value);

		if (pair->key == 49)
			service = pair->value;


		l = l->next;
	}

	if(pkt->service == YAHOO_SERVICE_P2PFILEXFER) {
		if(strcmp("FILEXFER", service) != 0) {
//                        fprintf(stderr, "unhandled service 0x%02x\n", pkt->service);
			yahoo_dump_unhandled(pkt);
			return;
		}
	}

	if(msg) {
		char *tmp;
		tmp = strchr(msg, '\006');
		if(tmp)
			*tmp = '\0';
	}
	if(url && from && ycb->yahoo_got_file)
		ycb->yahoo_got_file(yd->client_id, from, url, expires, msg, filename, filesize);

}

static void yahoo_process_conference(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *host = NULL;
	char *who = NULL;
	char *room = NULL;
	char *id = NULL;
	char **members = NULL;
	int nmembers = 0;
	GSList *l = pkt->hash;
	
	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 50)
			host = pair->value;
		
		if (pair->key == 52) {          // invite
			who = pair->value;
			if(members) {
				members = (char **)realloc(members, sizeof(char *) * (nmembers + 2));
			} else {
				members = (char **)malloc(sizeof(char *) * (nmembers + 2));
			}
			members[nmembers++] = strdup(who);
			members[nmembers] = NULL;
		}
		if (pair->key == 53)            // logon
			who = pair->value;
		if (pair->key == 54)            // decline
			who = pair->value;
		if (pair->key == 56)            // logoff
			who = pair->value;

		if (pair->key == 57)
			room = pair->value;

		if (pair->key == 58)            // join message
			msg = pair->value;
		if (pair->key == 14)            // decline/conf message
			msg = pair->value;

		if (pair->key == 13)
			;
		if (pair->key == 1)             // my id
			id = pair->value;
		if (pair->key == 3)             // message sender
			who = pair->value;

		l = l->next;
	}

	if(!room)
		return;

	if(host) {
		if(members) {
			members = (char **)realloc(members, sizeof(char *) * (nmembers + 2));
		} else {
			members = (char **)malloc(sizeof(char *) * (nmembers + 2));
		}
		members[nmembers++] = strdup(host);
		members[nmembers] = NULL;
	}
	// invite, decline, join, left, message -> status == 1

	switch(pkt->service) {
	case YAHOO_SERVICE_CONFINVITE:
	case YAHOO_SERVICE_CONFADDINVITE:
		if(ycb->yahoo_got_conf_invite)
			ycb->yahoo_got_conf_invite(yd->client_id, host, room, msg, members);
		break;
	case YAHOO_SERVICE_CONFDECLINE:
		if(who && ycb->yahoo_conf_userdecline)
			ycb->yahoo_conf_userdecline(yd->client_id, who, room, msg);
		break;
	case YAHOO_SERVICE_CONFLOGON:
		if(who && ycb->yahoo_conf_userjoin)
			ycb->yahoo_conf_userjoin(yd->client_id, who, room);
		break;
	case YAHOO_SERVICE_CONFLOGOFF:
		if(who && ycb->yahoo_conf_userleave)
			ycb->yahoo_conf_userleave(yd->client_id, who, room);
		break;
	case YAHOO_SERVICE_CONFMSG:
		if(who && ycb->yahoo_conf_message)
			ycb->yahoo_conf_message(yd->client_id, who, room, msg);
		break;
	}
}

static void yahoo_process_message(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *msg = NULL;
	char *from = NULL;
	long tm = 0L;
	GSList *l = pkt->hash;
	
	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 4)
			from = pair->value;
		else if (pair->key == 14)
			msg = pair->value;
		else if (pair->key == 15)
			tm = strtol(pair->value, NULL, 10);
		else if (pair->key == 16)       // system message
			msg = pair->value;
		else {};
//                        fprintf(stderr, "yahoo_process_message: status: %d, key: %d, value: %s\n",
//                                        pkt->status, pair->key, pair->value);
		l = l->next;
	}

	if (pkt->service == YAHOO_SERVICE_SYSMESSAGE) {
		if(ycb->yahoo_system_message)
			ycb->yahoo_system_message(yd->client_id, msg);
	} else if (pkt->status <= 1 || pkt->status == 5) {
		char *m;
		int i, j;
		//strip_linefeed(msg);
		m = msg;
		for (i = 0, j = 0; m[i]; i++) {
			if (m[i] == 033) {
				while (m[i] && (m[i] != 'm'))
					i++;
				if (!m[i])
					i--;
				continue;
			}
			msg[j++] = m[i];
		}
		msg[j] = 0;
		if(ycb->yahoo_got_im)
			ycb->yahoo_got_im(yd->client_id, from, msg, tm, (int)pkt->status);
	} else if (pkt->status == 2) {
		if(ycb->yahoo_got_im)
			ycb->yahoo_got_im(yd->client_id, from, NULL, tm, 2);
	} else if (pkt->status == 0xffffffff) {
		if(ycb->yahoo_error)
			ycb->yahoo_error(yd->client_id, msg, 0);
	}
}


static void yahoo_process_status(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;
	char *name = NULL;
	int state = 0;
	int away = 0;
	int gamestate = 0;
	char *msg = NULL;
	
	while (l) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 0: /* we won't actually do anything with this */
//                        fprintf(stderr, "key %d:%s\n", pair->key, pair->value);
			break;
		case 1: /* we don't get the full buddy list here. */
			if (!yd->logged_in) {
				yd->logged_in = TRUE;
				if(yd->current_status < 0)
					yd->current_status = yd->initial_status;
				if(ycb->yahoo_login_done)
					ycb->yahoo_login_done(yd->client_id, YAHOO_LOGIN_OK, NULL);
			}
			break;
		case 8: /* how many online buddies we have */
//                        fprintf(stderr, "key %d:%s\n", pair->key, pair->value);
			break;
		case 7: /* the current buddy */
			name = pair->value;
			break;
		case 10: /* state */
			state = strtol(pair->value, NULL, 10);
			break;
		case 19: /* custom message */
			msg = pair->value;
			break;
		case 47:
			away = atoi(pair->value);
			break;
		case 11: /* i didn't know what this was in the old protocol either */
//                        fprintf(stderr, "key %d:%s\n", pair->key, pair->value);
			break;
		case 17: /* in chat? */
			break;
		case 13: /* in pager? */
			if (pkt->service == YAHOO_SERVICE_LOGOFF || strtol(pair->value, NULL, 10) == 0) {
				// set buddy's status to OFFLINE
				if(ycb->yahoo_status_changed)
					ycb->yahoo_status_changed(yd->client_id, name, YAHOO_STATUS_OFFLINE, NULL, 1);
				break;
			}
			if (g_hash_table_lookup(yd->games, name))
				gamestate = YAHOO_STATUS_GAME;
			if (state == YAHOO_STATUS_AVAILABLE) {
				// if state is available, and he's gaming, then say so
				// else set his status as it is
				if(ycb->yahoo_status_changed)
					ycb->yahoo_status_changed(yd->client_id, name, gamestate?gamestate:state, NULL, 0);
			} else if (state == YAHOO_STATUS_CUSTOM) {
				gpointer val = g_hash_table_lookup(yd->hash, name);
				if (val) {
					g_free(val);
					g_hash_table_insert(yd->hash, name,
							msg ? g_strdup(msg) : g_malloc0(1));
				} else
					g_hash_table_insert(yd->hash, g_strdup(name),
							msg ? g_strdup(msg) : g_malloc0(1));
				if(ycb->yahoo_status_changed)
					ycb->yahoo_status_changed(yd->client_id, name, state, msg, away);
			} else {
				if(ycb->yahoo_status_changed)
					ycb->yahoo_status_changed(yd->client_id, name, state, NULL, 1);
			}

			break;
		case 60: /* no clue */
			/* sometimes going offline makes this 2, but invisible never sends it */
//                        fprintf(stderr, "key %d:%s\n", pair->key, pair->value);
			 break;
		case 16: /* Custom error message */
			if(ycb->yahoo_error)
				ycb->yahoo_error(yd->client_id, pair->value, 0);
			break;
		default:
//                        fprintf(stderr, "unknown status key %d:%s\n", pair->key, pair->value);
			break;
		}

		l = l->next;
	}
}

static void yahoo_process_list(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		l = l->next;

		if (pair->key == 87) {
			if(!yd->rawbuddylist)
				yd->rawbuddylist = g_strdup(pair->value);
			else {
				int len = strlen(yd->rawbuddylist) + strlen(pair->value);
				yd->rawbuddylist = g_renew(unsigned char, yd->rawbuddylist, len+1);
				strcat(yd->rawbuddylist, pair->value);
			}
		} else if (pair->key == 59) {
			if(yd->rawbuddylist) {
				yd->buddies = getbuddylist(yd->rawbuddylist);
				if(ycb->yahoo_got_buddies)
					ycb->yahoo_got_buddies(yd->client_id, yd->buddies);
				g_free(yd->rawbuddylist);
				yd->rawbuddylist = NULL;
			}
			if(pair->value[0]=='Y') {
				g_free(yd->cookie_y);
				g_free(yd->login_cookie);

				yd->cookie_y = getcookie(pair->value);
				yd->login_cookie = getlcookie(yd->cookie_y);

			} else if(pair->value[0]=='T') {
				g_free(yd->cookie_t);
				yd->cookie_t = getcookie(pair->value);
			} 
		}
	}
}

static void yahoo_process_auth(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *seed = NULL;
	char *sn   = NULL;
	GSList *l = pkt->hash;
	
	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 94)
			seed = pair->value;
		if (pair->key == 1)
			sn = pair->value;
		l = l->next;
	}
	
	if (!seed)
		return;

	
	/* So, Yahoo has stopped supporting its older clients in India, and undoubtedly
	 * will soon do so in the rest of the world.
	 * 
	 * The new clients use this authentication method.  I warn you in advance, it's
	 * bizzare, convoluted, inordinately complicated.  It's also no more secure than
	 * crypt() was.  The only purpose this scheme could serve is to prevent third
	 * part clients from connecting to their servers.
	 *
	 * Sorry, Yahoo.
	 */
	{

	struct yahoo_packet *pack;
	
	md5_byte_t result[16];
	md5_state_t ctx;
	char *crypt_result;
	char *password_hash = g_malloc(25);
	char *crypt_hash = g_malloc(25);
	char *hash_string_p = g_malloc(50 + strlen(sn));
	char *hash_string_c = g_malloc(50 + strlen(sn));
	
	char checksum;
	
	int sv;
	
	char *result6 = g_malloc(25);
	char *result96 = g_malloc(25);

	sv = seed[15];
	sv = sv % 8;

	md5_init(&ctx);
	md5_append(&ctx, yd->password, strlen(yd->password));
	md5_finish(&ctx, result);
	to_y64(password_hash, result, 16);
	
	md5_init(&ctx);
	crypt_result = yahoo_crypt(yd->password, "$1$_2S43d5f$");  
	md5_append(&ctx, crypt_result, strlen(crypt_result));
	md5_finish(&ctx, result);
	to_y64(crypt_hash, result, 16);

	switch (sv) {
	case 1:
	case 6:
		checksum = seed[seed[9] % 16];
		g_snprintf(hash_string_p, strlen(sn) + 50,
				"%c%s%s%s", checksum, yd->user, seed, password_hash);
		g_snprintf(hash_string_c, strlen(sn) + 50,
				"%c%s%s%s", checksum, yd->user, seed, crypt_hash);
		break;
	case 2:
	case 7:
		checksum = seed[seed[15] % 16];
		g_snprintf(hash_string_p, strlen(sn) + 50,
				"%c%s%s%s", checksum, seed, password_hash, yd->user);
		g_snprintf(hash_string_c, strlen(sn) + 50,
				"%c%s%s%s", checksum, seed, crypt_hash, yd->user);
		break;
	case 3:
		checksum = seed[seed[1] % 16];
		g_snprintf(hash_string_p, strlen(sn) + 50,
				"%c%s%s%s", checksum, yd->user, password_hash, seed);
	       g_snprintf(hash_string_c, strlen(sn) + 50,
				"%c%s%s%s", checksum, yd->user, crypt_hash, seed);
		break;
	case 4:
		checksum = seed[seed[3] % 16];
		g_snprintf(hash_string_p, strlen(sn) + 50,
				"%c%s%s%s", checksum, password_hash, seed, yd->user);
		g_snprintf(hash_string_c, strlen(sn) + 50,
				"%c%s%s%s", checksum, crypt_hash, seed, yd->user);
		break;
	case 0:
	case 5:
		checksum = seed[seed[7] % 16];
		g_snprintf(hash_string_p, strlen(sn) + 50,
				"%c%s%s%s", checksum, password_hash, yd->user, seed);
		g_snprintf(hash_string_c, strlen(sn) + 50,
				"%c%s%s%s", checksum, crypt_hash, yd->user, seed);
		break;
	}
		
	md5_init(&ctx);  
	md5_append(&ctx, hash_string_p, strlen(hash_string_p));
	md5_finish(&ctx, result);
	to_y64(result6, result, 16);

	md5_init(&ctx);  
	md5_append(&ctx, hash_string_c, strlen(hash_string_c));
	md5_finish(&ctx, result);
	to_y64(result96, result, 16);

	pack = yahoo_packet_new(YAHOO_SERVICE_AUTHRESP, yd->initial_status, 0);
	yahoo_packet_hash(pack, 0, yd->user);
	yahoo_packet_hash(pack, 6, result6);
	yahoo_packet_hash(pack, 96, result96);
	yahoo_packet_hash(pack, 1, yd->user);
		
	yahoo_send_packet(yd, pack, 0);
		
	g_free(password_hash);
	g_free(crypt_hash);
	g_free(hash_string_p);
	g_free(hash_string_c);

	yahoo_packet_free(pack);

	}
}

static void yahoo_process_auth_resp(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *login_id;
	char *handle;
	char *url=NULL;
	int  login_status=0;

	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 0)
			login_id = pair->value;
		else if (pair->key == 1)
			handle = pair->value;
		else if (pair->key == 20)
			url = pair->value;
		else if (pair->key == 66)
			login_status = atoi(pair->value);
		l = l->next;
	}

	if(pkt->status == 0xffffffff) {
		if(ycb->yahoo_login_done)
			ycb->yahoo_login_done(yd->client_id, login_status, url);
		yahoo_logoff(yd->client_id);
	}
}

static void yahoo_process_mail(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *who = NULL;
	char *email = NULL;
	char *subj = NULL;
	int count = 0;
	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 9)
			count = strtol(pair->value, NULL, 10);
		else if (pair->key == 43)
			who = pair->value;
		else if (pair->key == 42)
			email = pair->value;
		else if (pair->key == 18)
			subj = pair->value;
		l = l->next;
	}

	if(ycb->yahoo_mail_notify) {
		if (who && email && subj) {
			char *from = g_strdup_printf("%s (%s)", who, email);
			ycb->yahoo_mail_notify(yd->client_id, from, subj, count);
			g_free(from);
		} else 
			ycb->yahoo_mail_notify(yd->client_id, NULL, NULL, count);
	}
}

static void yahoo_process_contact(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
	char *id = NULL;
	char *who = NULL;
	char *msg = NULL;
	char *name = NULL;
	int state = YAHOO_STATUS_AVAILABLE;
	int online = FALSE;
	int away = 0;

	GSList *l = pkt->hash;

	while (l) {
		struct yahoo_pair *pair = l->data;
		if (pair->key == 1)
			id = pair->value;
		else if (pair->key == 3)
			who = pair->value;
		else if (pair->key == 14)
			msg = pair->value;
		else if (pair->key == 7)
			name = pair->value;
		else if (pair->key == 10)
			state = strtol(pair->value, NULL, 10);
		else if (pair->key == 13)
			online = strtol(pair->value, NULL, 10);
		else if (pair->key == 47)
			away = strtol(pair->value, NULL, 10);
		l = l->next;
	}

	if (id && ycb->yahoo_contact_added)
		ycb->yahoo_contact_added(yd->client_id, id, who, msg);
	if (name && ycb->yahoo_status_changed) {
		if (state != YAHOO_STATUS_CUSTOM)
			ycb->yahoo_status_changed(yd->client_id, name, state, NULL, 0);
		else if (state == YAHOO_STATUS_CUSTOM) {
			gpointer val = g_hash_table_lookup(yd->hash, name);
			if (val) {
				g_free(val);
				g_hash_table_insert(yd->hash, name,
						msg ? g_strdup(msg) : g_malloc0(1));
			} else
				g_hash_table_insert(yd->hash, g_strdup(name),
						msg ? g_strdup(msg) : g_malloc0(1));
			ycb->yahoo_status_changed(yd->client_id, name, state, msg, away);
		}
	}
}


static void yahoo_packet_process(struct yahoo_data *yd, struct yahoo_packet *pkt)
{
//        fprintf(stderr, "yahoo_packet_process: 0x%02x\n", pkt->service);
	switch (pkt->service)
	{
	case YAHOO_SERVICE_USERSTAT:
	case YAHOO_SERVICE_LOGON:
	case YAHOO_SERVICE_LOGOFF:
	case YAHOO_SERVICE_ISAWAY:
	case YAHOO_SERVICE_ISBACK:
	case YAHOO_SERVICE_GAMELOGON:
	case YAHOO_SERVICE_GAMELOGOFF:
		yahoo_process_status(yd, pkt);
		break;
	case YAHOO_SERVICE_NOTIFY:
		yahoo_process_notify(yd, pkt);
		break;
	case YAHOO_SERVICE_MESSAGE:
	case YAHOO_SERVICE_GAMEMSG:
	case YAHOO_SERVICE_SYSMESSAGE:
		yahoo_process_message(yd, pkt);
		break;
	case YAHOO_SERVICE_NEWMAIL:
		yahoo_process_mail(yd, pkt);
		break;
	case YAHOO_SERVICE_NEWCONTACT:
		yahoo_process_contact(yd, pkt);
		break;
	case YAHOO_SERVICE_LIST:
		yahoo_process_list(yd, pkt);
		break;
	case YAHOO_SERVICE_AUTH:
		yahoo_process_auth(yd, pkt);
		break;
	case YAHOO_SERVICE_AUTHRESP:
		yahoo_process_auth_resp(yd, pkt);
		break;
	case YAHOO_SERVICE_CONFINVITE:
	case YAHOO_SERVICE_CONFADDINVITE:
	case YAHOO_SERVICE_CONFDECLINE:
	case YAHOO_SERVICE_CONFLOGON:
	case YAHOO_SERVICE_CONFLOGOFF:
	case YAHOO_SERVICE_CONFMSG:
		yahoo_process_conference(yd, pkt);
		break;
	case YAHOO_SERVICE_P2PFILEXFER:
	case YAHOO_SERVICE_FILETRANSFER:
		yahoo_process_filetransfer(yd, pkt);
		break;
	case YAHOO_SERVICE_IDLE:
	case YAHOO_SERVICE_IDACT:
	case YAHOO_SERVICE_IDDEACT:
	case YAHOO_SERVICE_MAILSTAT:
	case YAHOO_SERVICE_CHATINVITE:
	case YAHOO_SERVICE_CALENDAR:
	case YAHOO_SERVICE_NEWPERSONALMAIL:
	case YAHOO_SERVICE_ADDIDENT:
	case YAHOO_SERVICE_ADDIGNORE:
	case YAHOO_SERVICE_PING:
	case YAHOO_SERVICE_GROUPRENAME:
	case YAHOO_SERVICE_PASSTHROUGH2:
	case YAHOO_SERVICE_CHATLOGON:
	case YAHOO_SERVICE_CHATLOGOFF:
	case YAHOO_SERVICE_CHATMSG:
	case YAHOO_SERVICE_ADDBUDDY:
	case YAHOO_SERVICE_REMBUDDY:
	case YAHOO_SERVICE_VOICECHAT:
//                fprintf(stderr, "unhandled service 0x%02x\n", pkt->service);
		yahoo_dump_unhandled(pkt);
		break;
	default:
//                fprintf(stderr, "unknown service 0x%02x\n", pkt->service);
		yahoo_dump_unhandled(pkt);
		break;
	}
}

static struct yahoo_packet * yahoo_getdata(struct yahoo_data * yd)
{
	struct yahoo_packet *pkt;
	int pos = 0;
	int pktlen;

	debug_printf("rxlen is %d\n", yd->rxlen);
	if (yd->rxlen < YAHOO_PACKET_HDRLEN) {
		debug_printf("len < YAHOO_PACKET_HDRLEN\n");
		return NULL;
	}

	pos += 4; /* YMSG */
	pos += 2;
	pos += 2;

	pktlen = yahoo_get16(yd->rxqueue + pos); pos += 2;
	debug_printf("%d bytes to read, rxlen is %d\n", 
			pktlen, yd->rxlen);

	if (yd->rxlen < (YAHOO_PACKET_HDRLEN + pktlen)) {
		debug_printf("len < YAHOO_PACKET_HDRLEN + pktlen\n");
		return NULL;
	}

	debug_printf("reading packet\n");
	yahoo_packet_dump(yd->rxqueue, YAHOO_PACKET_HDRLEN + pktlen);

	pkt = yahoo_packet_new(0, 0, 0);

	pkt->service = yahoo_get16(yd->rxqueue + pos); pos += 2;
	pkt->status = yahoo_get32(yd->rxqueue + pos); pos += 4;
	debug_printf("Yahoo Service: 0x%02x Status: %d\n", pkt->service,
		       pkt->status);
	pkt->id = yahoo_get32(yd->rxqueue + pos); pos += 4;

	yd->id = pkt->id;

	yahoo_packet_read(pkt, yd->rxqueue + pos, pktlen);

	yd->rxlen -= YAHOO_PACKET_HDRLEN + pktlen;
	if (yd->rxlen) {
		char *tmp = g_memdup(yd->rxqueue + YAHOO_PACKET_HDRLEN 
				+ pktlen, yd->rxlen);
		g_free(yd->rxqueue);
		yd->rxqueue = tmp;
	} else {
		g_free(yd->rxqueue);
		yd->rxqueue = NULL;
	}

	return pkt;
}

void yahoo_pending(guint32 id, int fd)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char buf[1024];
	int len;

	debug_printf("callback\n");

	if(!yd)
		return;
	
	len = read(fd, buf, sizeof(buf));

	if (len <= 0) {
		debug_printf("len == %d (<= 0)\n", len);
		if(ycb->yahoo_error) {
			if(len == 0)
				snprintf(buf, 1024, "Server closed socket");
			else
				snprintf(buf, 1024, "Read error: %d(%s) len=%d",
						errno, strerror(errno), len);
			ycb->yahoo_error(id, buf, 1);
		}
		return;
	}

	yd->rxqueue = g_realloc(yd->rxqueue, len + yd->rxlen);
	memcpy(yd->rxqueue + yd->rxlen, buf, len);
	yd->rxlen += len;

	while ((pkt = yahoo_getdata(yd)) != NULL) {

		yahoo_packet_process(yd, pkt);

		yahoo_packet_free(pkt);
	}
}

guint32 yahoo_login(char *username, char *password, int initial)
{
	struct yahoo_data *yd;
	struct yahoo_packet *pkt;
	int fd;

	if(!ycb)
		return -10;

	fd = yahoo_connect(pager_host, atoi(pager_port));

	if(fd <= 0)
		return (guint32)fd;

	yd = g_new0(struct yahoo_data, 1);
	yd->fd = fd;

	yd->user = strdup(username);
	yd->password = strdup(password);

	yd->hash = g_hash_table_new(g_str_hash, g_str_equal);
	yd->games = g_hash_table_new(g_str_hash, g_str_equal);

	yd->initial_status = initial;
	yd->current_status = -1;

	yd->client_id = ++last_id;

	conn_list = add_to_list(conn_list, yd);

	pkt = yahoo_packet_new(YAHOO_SERVICE_AUTH, YAHOO_STATUS_AVAILABLE, 0);

	yahoo_packet_hash(pkt, 1, username);
//        fprintf(stderr, "Sending initial packet\n");
	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);

	return yd->client_id;
}


int yahoo_get_fd(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return 0;
	else
		return yd->fd;
}

static void yahoo_close(struct yahoo_data *yd) 
{
	conn_list = del_from_list(conn_list, yd);

	g_hash_table_foreach_remove(yd->hash, yahoo_destroy_hash, NULL);
	g_hash_table_destroy(yd->hash);
	g_hash_table_foreach_remove(yd->games, yahoo_destroy_hash, NULL);
	g_hash_table_destroy(yd->games);
	if (yd->fd >= 0)
		close(yd->fd);
	if (yd->rxqueue)
		g_free(yd->rxqueue);
	yd->rxlen = 0;
	yahoo_free_data(yd);
}

void yahoo_send_im(guint32 id, char *who, char *what, int len)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;

	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_MESSAGE, YAHOO_STATUS_OFFLINE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 14, what);

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_send_typing(guint32 id, char *who, int typ)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_NOTIFY, YAHOO_STATUS_TYPING, yd->id);

	yahoo_packet_hash(pkt, 49, "TYPING");
	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 14, " ");
	yahoo_packet_hash(pkt, 13, typ ? "1" : "0");
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 1002, "1");

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_set_away(guint32 id, enum yahoo_status state, char *msg, int away)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;
	int service;
	char s[4];

	if(!yd)
		return;

	if (msg) {
		yd->current_status = YAHOO_STATUS_CUSTOM;
	} else {
		yd->current_status = state;
	}

	if (yd->current_status == YAHOO_STATUS_AVAILABLE)
		service = YAHOO_SERVICE_ISBACK;
	else
		service = YAHOO_SERVICE_ISAWAY;
	pkt = yahoo_packet_new(service, yd->current_status, yd->id);
	g_snprintf(s, sizeof(s), "%d", yd->current_status);
	yahoo_packet_hash(pkt, 10, s);
	if (yd->current_status == YAHOO_STATUS_CUSTOM) {
		yahoo_packet_hash(pkt, 19, msg);
		yahoo_packet_hash(pkt, 47, away?"1":"0");
	}

	yahoo_send_packet(yd, pkt, 0);
	yahoo_packet_free(pkt);
}

void yahoo_logoff(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;

	if(!yd)
		return;

	if(yd->current_status == -1)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_LOGOFF, YAHOO_STATUS_AVAILABLE, yd->id);
	yd->current_status = -1;

	if (pkt) {
		yahoo_send_packet(yd, pkt, 0);
		yahoo_packet_free(pkt);
	}

	yahoo_close(yd);
}

void yahoo_refresh(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;

	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_USERSTAT, YAHOO_STATUS_AVAILABLE, yd->id);
	if (pkt) {
		yahoo_send_packet(yd, pkt, 0);
		yahoo_packet_free(pkt);
	}
}

void yahoo_keepalive(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt=NULL;
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_PING, YAHOO_STATUS_AVAILABLE, yd->id);
	yahoo_send_packet(yd, pkt, 0);
	yahoo_packet_free(pkt);
}

void yahoo_add_buddy(guint32 id, char *who, char *group)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;

	if(!yd)
		return;

	if (!yd->logged_in)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_ADDBUDDY, YAHOO_STATUS_AVAILABLE, yd->id);
	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, group);
	yahoo_send_packet(yd, pkt, 0);
	yahoo_packet_free(pkt);


	/* maybe this should come in the response to this call */
	{
	int buddycnt;
	struct yahoo_buddy **tmp_buddylist, **budd, *bud;
	
	/* count number of buddies in this list */
	for (buddycnt = 0, budd = yd->buddies; *budd; budd++)
		buddycnt++;
	buddycnt++;     /* one more */

	/* allocate the array to hold the list of buddy */
	tmp_buddylist = (struct yahoo_buddy **)g_malloc(
			sizeof(struct yahoo_buddy *) * (buddycnt + 1));

	/* Free and copy the old one if necessary */
	if (yd->buddies)
	{
		memcpy(tmp_buddylist, yd->buddies, (buddycnt + 1) * sizeof(struct yahoo_buddy *));
		g_free(yd->buddies);
	}

	yd->buddies = tmp_buddylist;

	bud = g_new0(struct yahoo_buddy, 1);
	bud->id = strdup(who);
	bud->group = strdup(group);
	bud->real_name = NULL;
	yd->buddies[buddycnt-1] = bud;
	yd->buddies[buddycnt] = NULL;
	}
}

void yahoo_remove_buddy(guint32 id, char *who, char *group)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt = NULL;

	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_REMBUDDY, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 7, who);
	yahoo_packet_hash(pkt, 65, group);
	yahoo_send_packet(yd, pkt, 0);
	yahoo_packet_free(pkt);

	/* maybe this has to come in the response to this call */
	{
	struct yahoo_buddy **buddy, *tbud;

	for(buddy = yd->buddies; *buddy; buddy++) {
		struct yahoo_buddy *bud = *buddy;
		if(!strcmp(bud->id, who) && !strcmp(bud->group, group)) {
			g_free(bud->id);
			g_free(bud->group);
			g_free(bud->real_name);
			g_free(bud);

			while(*(buddy+1)) {
			    *buddy = *(buddy+1);
			    buddy++;
			}

			*buddy = 0;
			break;
		}
	}
	}
}

void yahoo_conference_addinvite(guint32 id, char *who, char *room, char *msg)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFADDINVITE, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 51, who);
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 58, msg);
	yahoo_packet_hash(pkt, 13, "0");

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_conference_invite(guint32 id, char **who, char *room, char *msg)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char **users;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFINVITE, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	yahoo_packet_hash(pkt, 50, yd->user);
	for(users = who; *users; users++) {
		char *user = *users;
		yahoo_packet_hash(pkt, 52, user);
	}
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 58, msg);
	yahoo_packet_hash(pkt, 13, "0");

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_conference_logon(guint32 id, char **who, char *room)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char **users;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGON, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	for(users = who; *users; users++) {
		char *user = *users;
		yahoo_packet_hash(pkt, 3, user);
	}
	yahoo_packet_hash(pkt, 57, room);

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_conference_decline(guint32 id, char **who, char *room, char *msg)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char **users;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFDECLINE, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	for(users = who; *users; users++) {
		char *user = *users;
		yahoo_packet_hash(pkt, 3, user);
	}
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 14, msg);

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_conference_logoff(guint32 id, char **who, char *room)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char **users;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFLOGOFF, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	for(users = who; *users; users++) {
		char *user = *users;
		yahoo_packet_hash(pkt, 3, user);
	}
	yahoo_packet_hash(pkt, 57, room);

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_conference_message(guint32 id, char **who, char *room, char *msg)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_packet *pkt;
	char **users;
		
	if(!yd)
		return;

	pkt = yahoo_packet_new(YAHOO_SERVICE_CONFMSG, YAHOO_STATUS_AVAILABLE, yd->id);

	yahoo_packet_hash(pkt, 1, yd->user);
	for(users = who; *users; users++) {
		char *user = *users;
		yahoo_packet_hash(pkt, 53, user);
	}
	yahoo_packet_hash(pkt, 57, room);
	yahoo_packet_hash(pkt, 14, msg);

	yahoo_send_packet(yd, pkt, 0);

	yahoo_packet_free(pkt);
}

void yahoo_send_file(guint32 id, char *who, char *msg, 
		char *name, long size, int datafd)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	struct yahoo_data *nyd;
	struct yahoo_packet *pkt = NULL;
	char size_str[10];
	long content_length=0;
	char buff[1024];
	char url[255];

	if(!yd)
		return;

	nyd = g_new0(struct yahoo_data, 1);
	nyd->id = yd->id;
	nyd->client_id = yd->client_id;
	nyd->user = g_strdup(yd->user);
	nyd->cookie_y = g_strdup(yd->cookie_y);
	nyd->cookie_t = g_strdup(yd->cookie_t);
	nyd->hash = g_hash_table_new(g_str_hash, g_str_equal);
	nyd->games = g_hash_table_new(g_str_hash, g_str_equal);

	conn_list = add_to_list(conn_list, nyd);

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANSFER, YAHOO_STATUS_AVAILABLE, nyd->id);

	snprintf(size_str, 9, "%ld", size);

	yahoo_packet_hash(pkt, 0, nyd->user);
	yahoo_packet_hash(pkt, 5, who);
	yahoo_packet_hash(pkt, 14, msg);
	yahoo_packet_hash(pkt, 27, name);
	yahoo_packet_hash(pkt, 28, size_str);

	content_length = YAHOO_PACKET_HDRLEN + yahoo_packet_length(pkt);

	snprintf(url, 255, "http://%s:%s/notifyft", filetransfer_host, filetransfer_port);
	nyd->fd = yahoo_http_post(url, nyd, content_length + 4 + size);

	yahoo_send_packet(nyd, pkt, 8);
	yahoo_packet_free(pkt);

	g_snprintf(buff, sizeof(buff), "29");
	buff[2] = 0xc0;
	buff[3] = 0x80;
	
	write(nyd->fd, buff, 4);

	while(size>0) {
		int cnt, cnt2;
		cnt=read(datafd, buff, sizeof(buff));
		size -= cnt;
		while(cnt>0) {
			cnt2 = write(nyd->fd, buff, cnt);
			cnt -= cnt2;
		}
	}

	while(yahoo_tcp_readline(buff, sizeof(buff), nyd->fd) > 0) {
		if(!strcmp(buff, ""))
			break;
	}

	yahoo_pending(nyd->client_id, nyd->fd);

	yahoo_close(nyd);
}


enum yahoo_status yahoo_current_status(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return YAHOO_STATUS_OFFLINE;
	return yd->current_status;
}

struct yahoo_buddy **get_buddylist(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return NULL;
	return yd->buddies;
}

char **get_identities(guint32 id)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return NULL;
	return yd->identities;
}

char *get_cookie(guint32 id, char *which)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return NULL;
	if(!strncasecmp(which, "y", 1))
		return yd->cookie_y;
	if(!strncasecmp(which, "t", 1))
		return yd->cookie_t;
	if(!strncasecmp(which, "login", 5))
		return yd->login_cookie;
	return NULL;
}

int yahoo_get_url_handle(guint32 id, char *url)
{
	struct yahoo_data *yd = find_conn_by_id(conn_list, id);
	if(!yd)
		return 0;

	return yahoo_get_url_fd(url, yd, NULL, NULL);
}


void yahoo_register_callbacks(struct yahoo_callbacks *yc)
{
	if(ycb)
		g_free(ycb);
	ycb = yc;
}

