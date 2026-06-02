// #include "config.h"
#ifdef OS2_MOUSEDRIVER
#include <stdio.h>
#define INCL_VIO
#define INCL_KBD
#define INCL_MOU
#include <os2.h>
#include "aalib.h"
#include "aaint.h"


static int but=0;
static HMOU hMou;


static int os2_init(aa_context * c, int mode) {
  USHORT mask = 0x7F;
  USHORT status = 0x0;

  if (MouOpen(NULL,&hMou) && MouOpen("POINTER$", &hMou)) {
    hMou = -1;
    return 0;
  }
  MouDrawPtr(hMou);
  MouSetDevStatus(&status, hMou);
  MouSetEventMask(&mask, hMou);

  return 1;
}


static void os2_uninit(aa_context * c) {
  USHORT status = 0;
  MouSetDevStatus(&status, hMou);
  MouClose(hMou);
}


static void os2_getmouse(aa_context * c, int *x, int *y, int *b) {
  MOUEVENTINFO mouEvent;
  PTRLOC ptrLoc;
  USHORT mouwait = MOU_NOWAIT;

  MouGetPtrPos(&ptrLoc, hMou);
  MouReadEventQue(&mouEvent, &mouwait, hMou);
  *x = ptrLoc.col; *y = ptrLoc.row;
  if((mouEvent.fs==0)&&(mouEvent.col==0)&&(mouEvent.row==0)) { /* no event */
    *b = but;
    return;
  }
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

  *b = but;
}


/*
  m = 1; showmouse
  m = 0; hidemouse
  */
static void os2_cursormode(aa_context * c, int m) {
  USHORT statusHide = 0x0;  
  USHORT statusShow = 0x100;  
  if(!m) {
    MouSetDevStatus(&statusShow, hMou);
    MouDrawPtr(hMou);
  }  else
    MouSetDevStatus(&statusHide, hMou);
}


struct aa_mousedriver mouse_os2_d =
{
    "os2", "OS/2 Mouse driver 0.1",
    AA_MOUSEALLMASK|AA_HIDECURSOR,
    os2_init,
    os2_uninit,
    os2_getmouse,
    os2_cursormode,
};
#endif
