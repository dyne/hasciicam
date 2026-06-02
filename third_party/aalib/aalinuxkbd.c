/* Raw keyboard mode driver for linux console by Jan Hubicka.
 * This driver is based on:
 * librawkey v0.21 - (c) 1994, 1995 Russell Marks
 * This library may be freely used/copied/modified provided this copyright
 * notice is left intact.
 *
 * needs keymap support in kernel - been there since 0.99pl12 I think.
 */

// #include "config.h"
#ifdef LINUX_DRIVER
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/kd.h>		/* RAW mode stuff, etc. */
#include <linux/keyboard.h>	/* mainly for NR_KEYS */
#include <linux/vt.h>		/* for VT stuff - nah, really? :) */
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#ifdef GPM_MOUSEDRIVER
#include <gpm.h>
#endif
#include "aalib.h"
#include "aaint.h"
static struct termios oldios;

/* random keys not convered anywhere else below */
#define ESCAPE_KEY	0x01
#define ENTER_KEY	28
#define BACKSPACE	14
#define TAB_KEY		15

/* shifts */
#define LEFT_SHIFT	0x2A
#define RIGHT_SHIFT	0x36
#define LEFT_CTRL	0x1D
#define LEFT_ALT	0x38

/* NB: right ctrl sends 0xE0 then LEFT_CTRL, right alt sends 0xE0 then
 * LEFT_ALT. If you want to do any shift handling, you probably want to
 * just ignore 0xE0, and look for LEFT_CTRL and LEFT_ALT.
 * note that using scan_keyboard() and is_key_pressed() does this for you.
 */

/* function keys */

/* this macro lets you do things like FUNC_KEY(1), FUNC_KEY(2), etc. up to
 * FUNC_KEY(12).
 * don't use any side-effects with it.
 */
#define FUNC_KEY(z)	(0x3A+(z)+(((z)>10)?18:0))

/* cursors, pgup, pgdn, etc. */

#define CURSOR_LEFT	0x4B
#define CURSOR_RIGHT	0x4D
#define CURSOR_UP	0x48
#define CURSOR_DOWN	0x50
#define CURSORBLOCK_LEFT	105
#define CURSORBLOCK_RIGHT	106
#define CURSORBLOCK_UP	103
#define CURSORBLOCK_DOWN	108
#define KEYPAD_CENTER	0x4C	/* the '5' in the centre of the keypad */

#define INSERT_KEY	0x52
#define DELETE_KEY	0x53
#define HOME_KEY	0x47
#define END_KEY		0x4F
#define PAGE_UP		0x49
#define PAGE_DOWN	0x51

/* NB: the 'grey' cursors, pgup, pgdn etc. generate 0xE0 before sending the
 * above codes. The easiest way to deal with this is to ignore 0xE0. :)
 */

#define CAPS_LOCK	0x3A
#define NUM_LOCK	0x45
#define SCROLL_LOCK	0x46

/* PrintScreen generates E0, 2A, E0, 37.        (0x63?)
 * Pause generates E1, 10, 45.                        (0x77?)
 * I leave it up to you how to figure those two out properly,
 * but the easiest way is to ignore them. :-/
 */

#define GRAY_PLUS	0x4E
#define GRAY_MINUS	0x4A
#define GRAY_MULTIPLY	0x37	/* NB: also gen'd by PrtSc, see above */
#define GRAY_DIVIDE	0x36	/* NB: prefixed by 0xE0 */



/* for most other keys, you should use the keymap_trans() function to
 * convert the scancode to whatever the keymap would normally generate.
 */



/* prototypes */

/* NB: it is *vital* that you call rawmode_exit() when you finish, or
 * else you'll be left with the keyboard translation in RAW mode! Not Good.
 * Consider setting up a SIGSEGV handler that calls it, etc. just in case.
 */

static int keymap[2][NR_KEYS];	/* scancode -> ASCII translation */
static int tty_fd = -1, restart_con, alt_pressed;
static struct termios new_termio, old_termio;
static int vtswitch_allowed;

static char key_down[128];
static int closed = 1;
static int mypid;


/* it's really easy to translate the scancodes these days, we just
 * use the keytable stuff!
 *
 * returns 0 on error, 1 if ok
 */
static int get_keyb_map(void)
{
    static struct kbentry keyb_ent;
    int f;

    keyb_ent.kb_table = 0;	/* unshifted */
    for (f = 0; f < NR_KEYS; f++) {
	keyb_ent.kb_index = f;

	if (ioctl(tty_fd, KDGKBENT, (unsigned int) &keyb_ent))
	    return (0);

	keymap[0][f] = keyb_ent.kb_value;
    }
    keyb_ent.kb_table = 1;	/* unshifted */
    for (f = 0; f < NR_KEYS; f++) {
	keyb_ent.kb_index = f;

	if (ioctl(tty_fd, KDGKBENT, (unsigned int) &keyb_ent))
	    return (0);

	keymap[1][f] = keyb_ent.kb_value;
    }
    return (1);
}




static void allow_switch(int on)
{
    vtswitch_allowed = on;
}

static void raw_mode(int tty_fd, int on)
{
    ioctl(tty_fd, KDSKBMODE, on ? K_MEDIUMRAW : K_XLATE);
}


static void blank_key_down(void)
{
    int f;

    for (f = 0; f < NR_KEYS; f++)
	key_down[f] = 0;
}


static void vt_from_here(int num)
{
    ioctl(tty_fd, TCSETSW, &old_termio);
    raw_mode(tty_fd, 0);	/* don't use rawmode_exit 'cos of other things it does */
    ioctl(tty_fd, VT_RELDISP, VT_ACKACQ);
    signal(SIGUSR1, vt_from_here);
}


static void vt_to_here(int num)
{
    struct termios ios;
    ioctl(tty_fd, TCSETSW, &new_termio);
    restart_con = 1;		/* we're back, say to start up again */
    alt_pressed = 0;
    raw_mode(tty_fd, 1);
    ios = oldios;
    ios.c_lflag &= ~ECHO;
    tcsetattr(tty_fd, 0, &ios);
    blank_key_down();
    signal(SIGUSR2, vt_to_here);
}


/* returns 1 if ok, 0 otherwise */
static int rawmode_init(void)
{
    if (!closed)
       return (1);
    mypid = getpid();
    if (tty_fd == -1) {
	tty_fd = fileno(stdin);
	fcntl(tty_fd, F_SETFL, O_NONBLOCK);
    }
/* fix termio stuff so ^C-style interrupts are ignored */
    ioctl(tty_fd, TCGETS, &old_termio);
    new_termio = old_termio;
    new_termio.c_lflag &= ~(ISIG | ICANON);
    ioctl(tty_fd, TCSETSW, &new_termio);

    if (get_keyb_map()) {
	struct vt_mode vtm;
	struct termios ios;

	blank_key_down();
	raw_mode(tty_fd, 1);
	signal(SIGUSR1, vt_from_here);
	signal(SIGUSR2, vt_to_here);
	ioctl(tty_fd, VT_GETMODE, &vtm);
	vtm.mode = VT_PROCESS;
	vtm.relsig = SIGUSR1;
	vtm.acqsig = SIGUSR2;
	ioctl(tty_fd, VT_SETMODE, &vtm);

	tcgetattr(tty_fd, &oldios);
	ios = oldios;
	ios.c_lflag &= ~ECHO;
	tcsetattr(tty_fd, 0, &ios);
	closed = 0;

	return (1);
    } else
	return (0);
}


static void rawmode_exit(void)
{
    struct vt_mode vtm;

    if (mypid != getpid())
	return;
    if (closed)
	return;
    closed = 1;
    raw_mode(tty_fd, 0);
    ioctl(tty_fd, VT_GETMODE, &vtm);
    vtm.mode = VT_AUTO;
    ioctl(tty_fd, VT_SETMODE, &vtm);
    ioctl(tty_fd, TCSETSW, &old_termio);
    fcntl(tty_fd, F_SETFL, 0);	/* allow for old versions of bash */
    tty_fd = -1;
    tcsetattr(tty_fd, 0, &oldios);
}


/* returns -1 if no keypresses pending, else returns scancode. */
static int get_scancode(void)
{
    unsigned char c;

    if (read(tty_fd, &c, 1) <= 0)
	return (-1);

    return ((int) c);
}


/* this is the routine you should call whenever you would normally
 * read a keypress. However, to actually tell if a key is pressed,
 * call is_key_pressed() with a scancode as arg.
 */
static int scan_keyboard(void)
{
    int c, key, flag;

/* we use BFI to fix the PrtSc/Pause problem - i.e. we don't :^) */
    while ((c = get_scancode()) == 0xE0);
    if (c == 0xE1)
	c = get_scancode();

    if (c == -1)
	return -1;		/* no key was pressed */

    key = c & 127;
    flag = (c & 128) ? 0 : 1;	/* 1 = down */

    if (flag || key_down[key] != flag)
      key_down[key] = flag;
    else
      return (scan_keyboard ());

    if (key == LEFT_ALT)
	alt_pressed = flag;

    if (alt_pressed && flag && key >= FUNC_KEY(1) && key <= FUNC_KEY(10)) {
	struct vt_stat vts;
	int newvt;

	ioctl(tty_fd, VT_GETSTATE, &vts);
	newvt = c - FUNC_KEY(1) + 1;
	if (vts.v_active != newvt && vtswitch_allowed) {
	    ioctl(tty_fd, VT_ACTIVATE, newvt);
	    restart_con = 0;
	    while (restart_con == 0)
		usleep(50000);
	}
	return -1;		/* Got VT switch */
    }
    if (flag && key == 46 && key_down[LEFT_CTRL])
	raise(SIGINT);
    return key;			/* No VT switch */
}


/* converts scancode to key binding */
static int keymap_trans(int sc)
{
    if (sc < 0 || sc > 127)
	return (-1);
    return (keymap[key_down[LEFT_SHIFT] || key_down[RIGHT_SHIFT]][sc]);
}





static int iswaiting;
static int __resized;
#ifdef GPM_MOUSEDRIVER
extern int __curses_usegpm;
#endif
static jmp_buf buf;
#ifdef SIGWINCH
static void handler(int i)
{
    __resized = 2;
    signal(SIGWINCH, handler);
    if (iswaiting)
	longjmp(buf, 1);
}
#endif
static char sig2catch[] =
{SIGHUP, SIGINT, SIGQUIT, SIGILL,
 SIGTRAP, SIGIOT, SIGBUS, SIGFPE,
 SIGSEGV, SIGPIPE, SIGALRM, SIGTERM,
 SIGXCPU, SIGXFSZ, SIGVTALRM,
	   /* SIGPROF , */ SIGPWR};
static struct sigaction old_signal_handler[sizeof(sig2catch)];
static void exithandler(int v)
{
    int i;
    printf("AAlib: signal %i received\n", v);
    rawmode_exit();
    for (i = 0; i < (int) sizeof(sig2catch); i++)
	if (sig2catch[i] == v) {
	    sigaction(v, old_signal_handler + i, NULL);
	    raise(v);
	    break;
	}
    if (i >= (int) sizeof(sig2catch)) {
	printf("AA-lib: Aieeee! Illegal call to signal_handler, raising segfault.\n");
	raise(SIGSEGV);
    }
}

static int linux_init(struct aa_context *context, int mode)
{
    int i;
    struct sigaction siga;
    if (!(mode & AA_SENDRELEASE))
	return 0;
    if (!rawmode_init())
	return 0;
#ifdef SIGWINCH
    signal(SIGWINCH, handler);
#endif
#ifdef GPM_MOUSEDRIVER
    aa_recommendlowmouse("gpm");
#endif
    allow_switch(1);
    atexit(rawmode_exit);
    for (i = 0; i < (int) sizeof(sig2catch); i++) {
	siga.sa_handler = exithandler;
	siga.sa_flags = 0;
	/*zero_sa_mask(&(siga.sa_mask)); */
	memset(&siga.sa_mask, 0, sizeof(sigset_t));
	sigaction((int) sig2catch[i], &siga, old_signal_handler + i);
    }

    return 1;
}
static void linux_uninit(aa_context * c)
{
#ifdef SIGWINCH
    signal(SIGWINCH, SIG_IGN);	/*this line may cause problem... */
#endif
    rawmode_exit();
}
#ifdef GPM_MOUSEDRIVER
extern int __gpm_user_handler(Gpm_Event * event, void *data);
#endif
static int linux_getchar(aa_context * c1, int wait)
{
#ifdef GPM_MOUSEDRIVER
    static Gpm_Event e;
#endif
    int c;
    int key;
    struct timeval tv;
    do {
	fd_set readfds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(tty_fd, &readfds);
#ifdef GPM_MOUSEDRIVER
	if (gpm_visiblepointer)
	    GPM_DRAWPOINTER(&e);
	if (__curses_usegpm) {
	    FD_SET(gpm_fd, &readfds);
	}
#endif
#ifdef GPM_MOUSEDRIVER
	select((__curses_usegpm ? gpm_fd : 0) + 1, &readfds, NULL, NULL, wait ? NULL : &tv);
	if (__curses_usegpm && FD_ISSET(gpm_fd, &readfds)) {
	    if (Gpm_GetEvent(&e) == 1) {
		__gpm_user_handler(&e, NULL);
		return AA_MOUSE;
	    }
	}
#else
	select(tty_fd, &readfds, NULL, NULL, wait ? NULL : &tv);
#endif
	c = scan_keyboard();
	if (c != -1) {
	    switch (c) {
	    case ESCAPE_KEY:
		key = AA_ESC;
		break;
	    case ENTER_KEY:
		key = 13;
		break;
	    case BACKSPACE:
		key = AA_BACKSPACE;
		break;
	    case CURSOR_LEFT:
		key = AA_LEFT;
		break;
	    case CURSOR_RIGHT:
		key = AA_RIGHT;
		break;
	    case CURSOR_UP:
		key = AA_UP;
		break;
	    case CURSOR_DOWN:
		key = AA_DOWN;
		break;
	    case CURSORBLOCK_LEFT:
		key = AA_LEFT;
		break;
	    case CURSORBLOCK_RIGHT:
		key = AA_RIGHT;
		break;
	    case CURSORBLOCK_UP:
		key = AA_UP;
		break;
	    case CURSORBLOCK_DOWN:
		key = AA_DOWN;
		break;
	    default:
		key = keymap_trans(c) & 255;
	    }
	    if (!key_down[c])
		key |= AA_RELEASE;
	    return key;
	} else
	    key = AA_NONE;
    }
    while (wait);
    return AA_NONE;
}



__AA_CONST struct aa_kbddriver kbd_linux_d =
{
    "linux", "Linux console raw keyboard driver 1.0",
    AA_SENDRELEASE,
    linux_init,
    linux_uninit,
    linux_getchar,
};
#endif
