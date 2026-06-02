// #include "config.h"
#ifdef OS2_KBDDRIVER
#include <stdio.h>
#define INCL_VIO
#define INCL_KBD
#define INCL_MOU
#include <os2.h>
#include "aalib.h"
#include "aaint.h"

extern HMOU hMou;  /* from aaos2mou.c */
extern UCHAR  but;


static int os2_init(aa_context * c, int mode) {
  return 1;
}

static void os2_uninit(aa_context * c) {
}

static int check_keys(KBDKEYINFO k) {
  if(!(k.fbStatus&0x02)) {
    if(k.chChar==8) return AA_BACKSPACE;
    if(k.chChar==27) return AA_ESC;
    return (k.chChar);
  }
  if(k.fbStatus&0x02) {
    switch(k.chScan) {
    case 72:
      return AA_UP;
    case 80:
      return AA_DOWN;
    case 75:
      return AA_LEFT;
    case 77:
      return AA_RIGHT;
    default:
      return AA_UNKNOWN;
    }
  } 
  return (AA_NONE);
}


static int os2_getchar(aa_context * c, int wait) {
  KBDKEYINFO kbdkey;
  MOUEVENTINFO mouEvent;
  USHORT mouwait = MOU_NOWAIT;

  if(wait) {
    while(1) {
      KbdCharIn(&kbdkey, IO_NOWAIT, 0);
      if(kbdkey.fbStatus!=0)
	return check_keys(kbdkey);
      if(hMou!=-1) {
	MouReadEventQue(&mouEvent, &mouwait, hMou);
	/* mouse event! */
	if(!((mouEvent.fs==0)&&(mouEvent.col==0)&&(mouEvent.row==0))) {
          if(mouEvent.fs&0x60)
	    but |= AA_BUTTON2;
  	  else
    	    but &= ~AA_BUTTON2;
  	  if((mouEvent.fs&0x18)||(mouEvent.fs&0x60 && mouEvent.fs&0x06))
    	    but |= AA_BUTTON3;
  	  else
    	    but &= ~AA_BUTTON3;
  	  if(mouEvent.fs&0x06)
    	    but |= AA_BUTTON1;
  	  else
    	    but &= ~AA_BUTTON1;
  	  if(mouEvent.fs&0x01)
    	    but = 0;

	  return AA_NONE;
	}
      }
      DosSleep(0);
    }
  } else {
    KbdCharIn(&kbdkey, IO_NOWAIT, 0);
    if(kbdkey.fbStatus==0)
      return AA_NONE;
    else
      return check_keys(kbdkey);
  }
  return (AA_NONE);
}

__AA_CONST struct aa_kbddriver kbd_os2_d =
{
    "os2", "OS/2 Keyboard driver 0.1",
    0,
    os2_init,
    os2_uninit,
    os2_getchar,
};
#endif
