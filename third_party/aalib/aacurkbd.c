#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
// #include "config.h"
#ifdef CURSES_KBDDRIVER
#ifdef USE_NCURSES
#include <ncurses.h>
#else
#include <curses.h>
#endif
#ifdef GPM_MOUSEDRIVER
#include <gpm.h>
#endif
#include "aalib.h"
#include "aaint.h"
static int iswaiting;
static jmp_buf buf;
extern int __curses_is_up;
extern volatile int __resized_curses;
#ifdef GPM_MOUSEDRIVER
extern int __curses_usegpm;
#endif
static int uninitcurses;
int __curses_x, __curses_y, __curses_buttons;
int __curses_keyboard;

#ifdef SIGWINCH
static void handler(int i)
{
    __resized_curses = 2;
    signal(SIGWINCH, handler);
    if (iswaiting)
	longjmp(buf, 1);
}
#endif
static int curses_init(struct aa_context *context, int mode)
{
    if (!__curses_is_up) {
	fflush(stdout);
	if ((initscr()) == NULL)
	    return 0;
	__curses_is_up = 1;
	uninitcurses = 1;
    }
    __curses_keyboard = 1;
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
#ifdef SIGWINCH
    signal(SIGWINCH, handler);
#endif
#ifdef GPM_MOUSEDRIVER
    aa_recommendlowmouse("gpm");
#endif
    aa_recommendlowkbd ("linux");
    aa_recommendlowmouse("curses");
    return 1;
}
static void curses_uninit(aa_context * c)
{
    printf("uninitializing\n");
    keypad(stdscr, FALSE);
    nodelay(stdscr, FALSE);
#ifdef SIGWINCH
    signal(SIGWINCH, SIG_IGN);	/*this line may cause problem... */
#endif
    nocbreak();
    echo();
    __curses_keyboard = 0;
    nl();
    if (uninitcurses) {
	intrflush(stdscr, TRUE);
	wclear(stdscr);
	refresh();
	uninitcurses = 0;
	__curses_is_up = 0;
	endwin();
    }
}
static int curses_getchar(aa_context * c1, int wait)
{
    int c;
    if (wait) {
	nodelay(stdscr, FALSE);
	setjmp(buf);
	iswaiting = 1;
    } else
	nodelay(stdscr, TRUE);
    if (__resized_curses == 2) {
	__resized_curses = 1;
	return (AA_RESIZE);
    }
#ifdef GPM_MOUSEDRIVER
    if (__curses_usegpm) {
	c = Gpm_Wgetch(stdscr);
    } else
#endif
	c = wgetch(stdscr);
    if (c == 27)
	return (AA_ESC);
    if (c > 0 && c < 127 && c != 127)
	return (c);
    switch (c) {
    case ERR:
	return (AA_NONE);
    case KEY_LEFT:
	return (AA_LEFT);
    case KEY_RIGHT:
	return (AA_RIGHT);
    case KEY_UP:
	return (AA_UP);
    case KEY_DOWN:
	return (AA_DOWN);
#ifdef KEY_MOUSE
    case KEY_MOUSE:
#ifdef GPM_MOUSEDRIVER
	if (!__curses_usegpm)
#endif
	{
	    MEVENT m;
	    if (getmouse(&m) == OK) {
		__curses_x = m.x;
		__curses_y = m.y;
	    }
	    if (m.bstate & BUTTON1_PRESSED)
		__curses_buttons |= AA_BUTTON1;
	    if (m.bstate & BUTTON1_RELEASED)
		__curses_buttons &= ~AA_BUTTON1;
	    if (m.bstate & BUTTON2_PRESSED)
		__curses_buttons |= AA_BUTTON2;
	    if (m.bstate & BUTTON2_RELEASED)
		__curses_buttons &= ~AA_BUTTON2;
	    if (m.bstate & BUTTON3_PRESSED)
		__curses_buttons |= AA_BUTTON3;
	    if (m.bstate & BUTTON3_RELEASED)
		__curses_buttons &= ~AA_BUTTON3;
	}
	return (AA_MOUSE);
#endif
    case KEY_BACKSPACE:
    case 127:
	return (AA_BACKSPACE);
    }
    return (AA_UNKNOWN);
}


__AA_CONST struct aa_kbddriver kbd_curses_d =
{
    "curses", "Curses keyboard driver 1.0",
    0,
    curses_init,
    curses_uninit,
    curses_getchar,
};
#endif
