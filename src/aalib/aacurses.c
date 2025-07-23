#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
// #include "config.h"
#ifdef CURSES_DRIVER
#ifdef USE_NCURSES
#ifdef RENAMED_NCURSES
#include <curses.h>
#else
#include <ncurses.h>
#endif
#else
#include <curses.h>
#endif
#ifdef GPM_MOUSEDRIVER
#include <gpm.h>
#endif
#include "aalib.h"
#include "aaint.h"
__AA_CONST struct aa_driver curses_d;
int __curses_is_up;
int __resized_curses;
static int uninitcurses;

static int curses_init(__AA_CONST struct aa_hardware_params *p, __AA_CONST void *none, struct aa_hardware_params *dest, void **param)
{
    int a;
    if (!__curses_is_up) {
	fflush(stdout);
	if ((initscr()) == NULL)
	    return 0;
	__curses_is_up = 1;
	uninitcurses = 1;
    }
    dest->font=NULL;
    dest->supported=AA_NORMAL_MASK;
#ifdef HAVE_TERMATTRS
    a = termattrs();
    if (has_colors()) {
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_BLACK);
	dest->supported |= AA_DIM_MASK;
    } else {
	if (a & A_DIM)
	    dest->supported |= AA_DIM_MASK;
    }
    if (a & A_BOLD)
	dest->supported |= AA_BOLD_MASK;
    if (a & A_BOLD)
	dest->supported |= AA_BOLDFONT_MASK;
    if (a & A_REVERSE)
	dest->supported |= AA_REVERSE_MASK;
#else
    dest->supported |= AA_REVERSE_MASK | AA_DIM_MASK | AA_BOLD_MASK | AA_BOLDFONT_MASK;
#endif
    intrflush(stdscr, FALSE);
    aa_recommendlowkbd("curses");
    return 1;
}
static void curses_uninit(aa_context * c)
{
    wclear(stdscr);
    intrflush(stdscr, TRUE);
#if 0
    nocbreak();
    echo();
    nl();
#endif
    refresh();
    if (uninitcurses) {
	uninitcurses = 0;
	__curses_is_up = 0;
	endwin();
    }
}
static void curses_getsize(aa_context * c, int *width, int *height)
{
    if (__resized_curses)
	curses_uninit(c), curses_init(&c->params, NULL,&c->driverparams, NULL), __resized_curses = 0;
    *width = stdscr->_maxx + 1;
    *height = stdscr->_maxy + 1;
#ifdef GPM_MOUSEDRIVER
    gpm_mx = *width;
    gpm_my = *height;
#endif

}
static void curses_setattr(aa_context * c, int attr)
{
    switch (attr) {
    case AA_NORMAL:
	attrset(A_NORMAL);
	break;
    case AA_DIM:
	if (has_colors()) {
	    attrset(A_BOLD);
	    attron(COLOR_PAIR(1));
	} else {
	    attrset(A_DIM);
	}
	break;
    case AA_BOLD:
	attrset(A_BOLD);
	break;
    case AA_BOLDFONT:
	attrset(A_BOLD);
	break;
    case AA_REVERSE:
	attrset(A_REVERSE);
	break;
    case AA_SPECIAL:
	attrset(A_REVERSE);
	break;
    }
}
static void curses_print(aa_context * c, __AA_CONST char *text)
{
    addstr(text);
}
static void curses_flush(aa_context * c)
{
    refresh();
}
static void curses_gotoxy(aa_context * c, int x, int y)
{
    move(y, x);
}
static void curses_cursor(aa_context * c, int mode)
{
#ifdef HAVE_CURS_SET
    curs_set(mode);
#endif
}



__AA_CONST struct aa_driver curses_d =
{
    "curses", "Curses driver 1.0",
    curses_init,
    curses_uninit,
    curses_getsize,
    curses_setattr,
    curses_print,
    curses_gotoxy,
    /*curses_getchar,
       NULL, */
    curses_flush,
    curses_cursor,
};
#endif
