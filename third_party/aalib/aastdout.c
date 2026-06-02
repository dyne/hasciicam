// #include "config.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include "aalib.h"
#include "aaint.h"

static int stdout_init(__AA_CONST struct aa_hardware_params *p,__AA_CONST  void *none, struct aa_hardware_params *dest, void **n)
{
    __AA_CONST static struct aa_hardware_params def={NULL, AA_NORMAL_MASK | AA_EXTENDED};
    *dest=def;
    return 1;
}
static void stdout_uninit(aa_context * c)
{
}
static void stdout_getsize(aa_context * c, int *width, int *height)
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    (void)c;
    if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
        *width = info.srWindow.Right - info.srWindow.Left + 1;
        *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    } else {
        *width = 80;
        *height = 25;
    }
#else
    // Try to get terminal size using ioctl
    struct winsize ws;
    (void)c;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col > 0 ? ws.ws_col : 80;
        *height = ws.ws_row > 0 ? ws.ws_row : 25;
    } else {
        // Fallback to default size
        *width = 80;
        *height = 25;
    }
#endif
}

static void stdout_flush(aa_context * c)
{
    int x, y;
    for (y = 0; y < aa_scrheight(c); y++) {
	for (x = 0; x < aa_scrwidth(c); x++) {
	    putc(c->textbuffer[x + y * aa_scrwidth(c)], stdout);
	}
	putc('\n', stdout);
    }
    putc('', stdout);
    putc('\n', stdout);
    fflush(stdout);
}
static void stdout_gotoxy(aa_context * c, int x, int y)
{
}
__AA_CONST struct aa_driver stdout_d =
{
    "stdout", "Standard output driver",
    stdout_init,
    stdout_uninit,
    stdout_getsize,
    NULL,
    NULL,
    stdout_gotoxy,
    stdout_flush,
    NULL
};


static void stderr_flush(aa_context * c)
{
    int x, y;
    for (y = 0; y < aa_scrheight(c); y++) {
	for (x = 0; x < aa_scrwidth(c); x++) {
	    putc(c->textbuffer[x + y * aa_scrwidth(c)], stderr);
	}
	putc('\n', stderr);
    }
    putc('', stderr);
    putc('\n', stderr);
    fflush(stderr);
}
__AA_CONST struct aa_driver stderr_d =
{
    "stderr", "Standard error driver",
    stdout_init,
    stdout_uninit,
    stdout_getsize,
    NULL,
    NULL,
    stdout_gotoxy,
    stderr_flush,
    NULL
};
