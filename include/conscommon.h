#ifndef __KONST_UI_FUNC_H_
#define __KONST_UI_FUNC_H_

#include "kkstrtext.h"
#include "conf.h"

#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ncurses.h>

#undef box
#undef clear
#undef erase
#undef move
#undef refresh

/* Fucking ncurses stuff */

#define boldcolor(c)    COLOR_PAIR(c)|A_BOLD
#define color(c)        COLOR_PAIR(c)

#define VLINE           kintf_graph ? ACS_VLINE         : '|'
#define HLINE           kintf_graph ? ACS_HLINE         : '-'
#define ULCORNER        kintf_graph ? ACS_ULCORNER      : '+'
#define URCORNER        kintf_graph ? ACS_URCORNER      : '+'
#define LLCORNER        kintf_graph ? ACS_LLCORNER      : '+'
#define LRCORNER        kintf_graph ? ACS_LRCORNER      : '+'
#define LTEE            kintf_graph ? ACS_LTEE          : '|'
#define RTEE            kintf_graph ? ACS_RTEE          : '|'
#define TTEE            kintf_graph ? ACS_TTEE          : '+'
#define BTEE            kintf_graph ? ACS_BTEE          : '+'

#define KEY_TAB 9
#define KEY_ESC 27

#ifndef CTRL
#define CTRL(x) ((x) & 0x1F)
#endif

#ifndef ALT
#define ALT(x) (0x200 | (unsigned int) x)
#endif

#define SHIFT_PRESSED   1
#define ALTR_PRESSED    2
#define ALTL_PRESSED    8
#define CONTROL_PRESSED 4

extern bool kintf_graph, kintf_refresh;

void printchar(char c);
void printstring(string s);
int string2key(const string adef);

const string makebidi(const string buf, int lpad = 0);

__KTOOL_BEGIN_C

void kinterface();
void kendinterface();
  
extern void (*kinputidle)(void);
char *kinput(unsigned int size, char *buf);
  
int keypressed();
int emacsbind(int k);

int getkey();
int getctrlkeys();

void kwriteatf(int x, int y, int c, const char *fmt, ...);
void kwriteat(int x, int y, const char *msg, int c);
void kgotoxy(int x, int y);
void hidecursor();
void showcursor();
void setbeep(int freq, int duration);
void delay(int milisec);
int kwherex();
int kwherey();

__KTOOL_END_C

#endif
