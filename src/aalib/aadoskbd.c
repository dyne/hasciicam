#include <stdio.h>
#include <conio.h>
#include <libtext.h>
#include <go32.h>
#include <dpmi.h>
#include <sys/nearptr.h>
#include <malloc.h>
#include <stdlib.h>
#include <dos.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include "aalib.h"
#include "aaint.h"

#define FALSE 0
#define TRUE 1
#define END_OF_FUNCTION(x)    void x##_end() { }
#define LOCK_VARIABLE(x)      _go32_dpmi_lock_data((void *)&x, sizeof(x))
#define LOCK_FUNCTION(x)      _go32_dpmi_lock_code(x, (long)x##_end - (long)x)

#define DISABLE()   asm volatile ("cli")
#define ENABLE()    asm volatile ("sti")

#define SCANCODE_TO_KEY(c)       (key_ascii_table[c])
#define SCANCODE_TO_CAPS(c)      (key_capslock_table[c])
#define SCANCODE_TO_SHIFT(c)     (key_shift_table[c])
#define SCANCODE_TO_CONTROL(c)   (key_control_table[c])
#define SCANCODE_TO_ALT(c)       (0)

#define KB_SHIFT_FLAG         1
#define KB_CTRL_FLAG          2
#define KB_ALT_FLAG           4
#define KB_LWIN_FLAG          8
#define KB_RWIN_FLAG          16
#define KB_MENU_FLAG          32
#define KB_SCROLOCK_FLAG      256
#define KB_NUMLOCK_FLAG       512
#define KB_CAPSLOCK_FLAG      1024
#define KB_INALTSEQ_FLAG      4096

#define KEY_ESC               1     /* keyboard scan codes  */
#define KEY_1                 2 
#define KEY_2                 3 
#define KEY_3                 4
#define KEY_4                 5
#define KEY_5                 6
#define KEY_6                 7
#define KEY_7                 8
#define KEY_8                 9
#define KEY_9                 10
#define KEY_0                 11
#define KEY_MINUS             12
#define KEY_EQUALS            13
#define KEY_BACKSPACE         14
#define KEY_TAB               15 
#define KEY_Q                 16
#define KEY_W                 17
#define KEY_E                 18
#define KEY_R                 19
#define KEY_T                 20
#define KEY_Y                 21
#define KEY_U                 22
#define KEY_I                 23
#define KEY_O                 24
#define KEY_P                 25
#define KEY_OPENBRACE         26
#define KEY_CLOSEBRACE        27
#define KEY_ENTER             28
#define KEY_CONTROL           29
#define KEY_LCONTROL          29
#define KEY_A                 30
#define KEY_S                 31
#define KEY_D                 32
#define KEY_F                 33
#define KEY_G                 34
#define KEY_H                 35
#define KEY_J                 36
#define KEY_K                 37
#define KEY_L                 38
#define KEY_COLON             39
#define KEY_QUOTE             40
#define KEY_TILDE             41
#define KEY_LSHIFT            42
#define KEY_BACKSLASH         43
#define KEY_Z                 44
#define KEY_X                 45
#define KEY_C                 46
#define KEY_V                 47
#define KEY_B                 48
#define KEY_N                 49
#define KEY_M                 50
#define KEY_COMMA             51
#define KEY_STOP              52
#define KEY_SLASH             53
#define KEY_RSHIFT            54
#define KEY_ASTERISK          55
#define KEY_ALT               56
#define KEY_SPACE             57
#define KEY_CAPSLOCK          58
#define KEY_F1                59
#define KEY_F2                60
#define KEY_F3                61
#define KEY_F4                62
#define KEY_F5                63
#define KEY_F6                64
#define KEY_F7                65
#define KEY_F8                66
#define KEY_F9                67
#define KEY_F10               68
#define KEY_NUMLOCK           69
#define KEY_SCRLOCK           70
#define KEY_HOME              71
#define KEY_UP                72
#define KEY_PGUP              73
#define KEY_MINUS_PAD         74
#define KEY_LEFT              75
#define KEY_5_PAD             76
#define KEY_RIGHT             77
#define KEY_PLUS_PAD          78
#define KEY_END               79
#define KEY_DOWN              80
#define KEY_PGDN              81
#define KEY_INSERT            82
#define KEY_DEL               83
#define KEY_F11               87
#define KEY_F12               88
#define KEY_LWIN              91
#define KEY_RWIN              92
#define KEY_MENU              93
#define KEY_PAD               100
#define KEY_RCONTROL          120
#define KEY_ALTGR             121


/* Most of code is borrowed from allegro - an grapics library for djgpp by
 *      By Shawn Hargreaves,
 *      1 Salisbury Road,
 *      Market Drayton,
 *      Shropshire,
 *      England, TF9 1AJ.
 *
 */
#define KEYBOARD_INT          9

#define KB_SPECIAL_MASK       0x3F
#define KB_CTRL_ALT_FLAG      (KB_CTRL_FLAG | KB_ALT_FLAG)


static int three_finger_flag = TRUE;
static int key_led_flag = TRUE;

static int keyboard_installed = FALSE; 

static volatile char key[128];                   /* key pressed flags */

static volatile int key_shifts = 0;


__AA_CONST static short key_ascii_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   AA_ESC,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', AA_BACKSPACE, 9,       /* 0 */
   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 13,  0,   'a', 's',     /* 1 */
   'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39,  '`', 0,   92,  'z', 'x', 'c', 'v',     /* 2 */
   'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   3,   3,   3,   3,   8,       /* 3 */
   3,   3,   3,   3,   3,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,       /* 4 */
   0,   0,   0,   127, 0,   0,   92,  3,   3,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   13,  0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   127,     /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


__AA_CONST static int key_capslock_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   AA_ESC,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', AA_BACKSPACE,   9,       /* 0 */
   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 13,  0,   'A', 'S',     /* 1 */
   'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', 39,  '`', 0,   92,  'Z', 'X', 'C', 'V',     /* 2 */
   'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   3,   3,   3,   3,   8,       /* 3 */
   3,   3,   3,   3,   3,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,       /* 4 */
   0,   0,   0,   127, 0,   0,   92,  3,   3,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   13,  0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   127,     /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


__AA_CONST static int key_shift_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   AA_ESC,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', AA_BACKSPACE, 126,     /* 0 */
   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 126, 0,   'A', 'S',     /* 1 */
   'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', 34,  '~', 0,   '|', 'Z', 'X', 'C', 'V',     /* 2 */
   'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   1,   0,   1,   1,   1,   1,   1,       /* 3 */
   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,       /* 4 */
   0,   0,   1,   127, 0,   0,   0,   1,   1,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   13,  0,   '/', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   127,     /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


__AA_CONST static int key_control_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   0,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   0,   0,   127, 127,     /* 0 */
   17,  23,  5,   18,  20,  25,  21,  9,   15,  16,  2,   2,   10,  0,   1,   19,      /* 1 */
   4,   6,   7,   8,   10,  11,  12,  0,   0,   0,   0,   0,   26,  24,  3,   22,      /* 2 */
   2,   14,  13,  0,   0,   0,   0,   0,   0,   0,   0,   2,   2,   2,   2,   2,       /* 3 */
   2,   2,   2,   2,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 4 */
   0,   0,   2,   0,   0,   0,   0,   2,   2,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


__AA_CONST static int key_numlock_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 1 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 2 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 3 */
   0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', 0,   '4', '5', '6', 0,   '1',     /* 4 */
   '2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


/*
  Mapping:
  0 = Use the second value as scan char, just like before.
  1 = Ignore the key.
  2 = Use the scan but put the shift flags instead of the ASCII.
  other = Use this value as scancode.

  Extended values:
  E0 1C = Enter
  E0 1D = RCtrl         => fake Ctrl
  E0 2A = ?????? generated in conjuntion with Insert!!
  E0 35 = \
  E0 38 = AltGr or RAlt => fake Alt
  E0 46 = Ctrl-Pause
  E0 47 = Home
  E0 48 = Up
  E0 4B = Left
  E0 4D = Right
  E0 4F = End
  E0 50 = Down
  E0 51 = Page-Down
  E0 52 = Insert
  E0 53 = Delete
*/
__AA_CONST static int key_extended_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   120, 0,   0,       /* 1 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   0,       /* 2 */
   0,   0,   0,   0,   0,   0,   0,   0,   121, 0,   0,   0,   0,   0,   0,   0,       /* 3 */
   0,   0,   0,   0,   0,   0,   2,   2,AA_UP,  2,   0,AA_LEFT,2,AA_RIGHT,0,  2,       /* 4 */
  AA_DOWN,2, 2,   2,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 5 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0        /* 7 */
};


__AA_CONST static int key_special_table[128] =
{
/* 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F             */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 0 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   2,   0,   0,       /* 1 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,   0,   0,   0,   0,       /* 2 */
   0,   0,   0,   0,   0,   0,   1,   0,   4,   0,1024,   0,   0,   0,   0,   0,       /* 3 */
   0,   0,   0,   0,   0, 512, 256,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 4 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   8,  16,  32,   0,   0,       /* 5 */
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,       /* 6 */
   0,   0,   0,   0,   0,   0,   0,   0,   2,   4,   0,   0,   0,   0,   0,   0        /* 7 */
};


#define KEY_BUFFER_SIZE    256

static volatile int key_buffer[KEY_BUFFER_SIZE]; 
static volatile int key_buffer_start = 0;
static volatile int key_buffer_end = 0;
static volatile int key_extended = 0;
static volatile int key_pad_seq;




/* add_key:
 *  Helper function to add a keypress to the buffer.
 */
static inline void add_key(int c)
{
   key_buffer[key_buffer_end] = c;

   key_buffer_end++;
   if (key_buffer_end >= KEY_BUFFER_SIZE)
      key_buffer_end = 0;
   if (key_buffer_end == key_buffer_start) {    /* buffer full */
      key_buffer_start++;
      if (key_buffer_start >= KEY_BUFFER_SIZE)
	 key_buffer_start = 0;
   }
}



/* clear_keybuf:
 *  Clears the keyboard buffer.
 */
static void clear_keybuf()
{
   int c;

   DISABLE();

   key_buffer_start = 0;
   key_buffer_end = 0;

   for (c=0; c<128; c++)
      key[c] = FALSE;

   ENABLE();

}



/* keypressed:
 *  Returns TRUE if there are keypresses waiting in the keyboard buffer.
 */
static int keypressed()
{
   if (key_buffer_start == key_buffer_end) {
	 return FALSE;
   }
   else
      return TRUE;
}



/* readkey:
 *  Returns the next character code from the keyboard buffer. If the
 *  buffer is empty, it waits until a key is pressed. The low byte of
 *  the return value contains the ASCII code of the key, and the high
 *  byte the scan code. 
 */
static int readkey()
{
   int r;

   if ((!keyboard_installed))
      return 0;


   do {
/*      printf("%i %i\n",key_buffer_start,key_buffer_end);*/
   } while (key_buffer_start == key_buffer_end);  /* wait for a press */

   DISABLE();

   r = key_buffer[key_buffer_start];
   key_buffer_start++;
   if (key_buffer_start >= KEY_BUFFER_SIZE)
      key_buffer_start = 0;

   ENABLE();

   return r;
}



/* kb_wait_for_write_ready:
 *  Wait for the keyboard controller to set the ready-for-write bit.
 */
static inline void kb_wait_for_write_ready()
{
   long i = 1000000L;

   while ((i--) && (inportb(0x64) & 2))
      ; /* wait */
}



/* kb_wait_for_read_ready:
 *  Wait for the keyboard controller to set the ready-for-read bit.
 */
static inline void kb_wait_for_read_ready()
{
   long i = 1000000L;

   while ((i--) && (!(inportb(0x64) & 0x01)))
      ; /* wait */
}



/* kb_send_data:
 *  Sends a byte to the keyboard controller. Returns 1 if all OK.
 */
static inline int kb_send_data(unsigned char data)
{
   long i;
   int resends = 4;
   int temp;

   do {
      kb_wait_for_write_ready();

      outportb(0x60, data);
      i = 2000000L;

      while (--i) {
	 kb_wait_for_read_ready();
	 temp = inportb(0x60);

	 if (temp == 0xFA)
	    return 1;
	 else if (temp == 0xFE)
	    break;
      }
   }
   while ((resends-- > 0) && (i));

   return 0;
}



/* update_leds:
 *  Sets the state of the keyboard LED indicators.
 */
static inline void update_leds()
{
   if ((!kb_send_data(0xED)) || (!kb_send_data((key_shifts>>8) & 7)))
      kb_send_data(0xF4);
}



/* my_keyint:
 *  Hardware level keyboard interrupt (int 9) handler.
 */
static int my_keyint()
{
   int t, temp, release, flag;
   char newchar;

   temp = inportb(0x60);            /* read keyboard byte */

   if (temp == 0xE0) {
      key_extended = 1; 
   }
   else {
      release = (temp & 0x80);      /* bit 7 means key was released */
      temp &= 0x7F;                 /* bits 0-6 is the scan code */

      if (key_extended) {           /* if is an extended code */
	 key_extended = 0;

	 if (((temp == KEY_END) || (temp == KEY_DEL)) && 
	     ((key_shifts & KB_CTRL_ALT_FLAG) == KB_CTRL_ALT_FLAG) && 
	     (!release) && (three_finger_flag)) {
	    asm (
	       "  movb $0x79, %%al ; "
	       "  call ___djgpp_hw_exception "
	    : : : "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi", "memory"
	    );
	    goto exit_keyboard_handler;
	 }

	 switch (key_extended_table[temp]) {

	    case 0:
	       /* process as normal */
	       break;

	    case 1:
	       /* ignore the key */
	       goto exit_keyboard_handler; 

	    case 2:
	       if (release) {
		  key[temp] = FALSE;
		  t = (temp+AA_UNKNOWN)|AA_RELEASE;
		  add_key(t);
	       }
	       else { 
		  /* report the scan code + the shift state */
		  key[temp] = TRUE;
		  t = temp+AA_UNKNOWN;
		  add_key(t);
	       }
	       goto exit_keyboard_handler;

	    default:
	       /* use a replacement value from the LUT */
	       temp = key_extended_table[temp];
               if(temp>255) {
	         if (release) {
		    key[temp] = FALSE;
		    t = temp|AA_RELEASE;
		    add_key(t);
	         }
	         else { 
		    /* report the scan code + the shift state */
		    key[temp] = TRUE;
		    t = temp;
		    add_key(t);
	         }
	       goto exit_keyboard_handler;
               }
	       break;
	 }
      } 

      if (release) {                /* key was released */
	 key[temp] = FALSE;

	 if ((flag = key_special_table[temp]) != 0) {
	    if ((flag < KB_SCROLOCK_FLAG) && (flag != KB_ALT_FLAG)) {
		key_shifts &= ~flag; 
	    }
	    else if (flag == KB_ALT_FLAG) {
	       key_shifts &= ~flag;
	       if (key_shifts & KB_INALTSEQ_FLAG) {
		  key_shifts &= ~KB_INALTSEQ_FLAG;
		  /*add_key(key_pad_seq & 0xFF);*/
	       }
	    }
	 } else goto addkeybuf;
      }
      else {                        /* key was pressed */
	 key[temp] = TRUE;

	 if ((flag = key_special_table[temp]) != 0) {
	    if (flag >= KB_SCROLOCK_FLAG) {
	       if (key_led_flag) {
		  key_shifts ^= flag;
		  update_leds();
		  goto exit_keyboard_handler;
	       }
	    }
	    else
	       key_shifts |= flag;
	 }
	 else {                     /* normal key */
	    if (key_shifts & KB_ALT_FLAG) {
	       if ((temp >= 0x47) && (key_extended_table[temp] == 2)) { 
		  if (key_shifts & KB_INALTSEQ_FLAG) {
		     key_pad_seq = key_pad_seq*10 + key_numlock_table[temp]-'0';
		  }
		  else {
		     key_shifts |= KB_INALTSEQ_FLAG;
		     key_pad_seq = key_numlock_table[temp] - '0';
		  }
		  goto exit_keyboard_handler;
	       }
	       else
		  t = SCANCODE_TO_ALT(temp);
	    }
	    else {
            addkeybuf:
            if (key_shifts & KB_CTRL_FLAG)
	       t = SCANCODE_TO_CONTROL(temp);
	    else if (key_shifts & KB_SHIFT_FLAG)
	       t = SCANCODE_TO_SHIFT(temp);
	    else if ((key_shifts & KB_NUMLOCK_FLAG) &&
		     (newchar = key_numlock_table[temp]) != 0)
	       t = (KEY_PAD<<8) | newchar;
	    else if (key_shifts & KB_CAPSLOCK_FLAG)
	       t = SCANCODE_TO_CAPS(temp);
	    else
	       t = SCANCODE_TO_KEY(temp);

	    key_shifts &= ~KB_INALTSEQ_FLAG;
            t&=65535;
            if(release) t|=AA_RELEASE;

	    add_key(t);
            }
	 }
      }
   }

   exit_keyboard_handler:

   outportb(0x20,0x20);       /* ack. the interrupt */
   return 0;
}

static END_OF_FUNCTION(my_keyint);



/* install_keyboard:
 *  Installs Allegro's keyboard handler. You must call this before using 
 *  any of the keyboard input routines. Note that Allegro completely takes 
 *  over the keyboard, so the debugger will not work properly, and under 
 *  DOS even ctrl-alt-del will have no effect. Returns -1 on failure.
 */
static _go32_dpmi_seginfo pmint,oldint;
static int install_keyboard()
{
   unsigned short shifts;

   if (keyboard_installed)
      return -1;

   LOCK_VARIABLE(three_finger_flag);
   LOCK_VARIABLE(key_led_flag);
   LOCK_VARIABLE(key_shifts);
   LOCK_VARIABLE(key);
   LOCK_VARIABLE(key_ascii_table);
   LOCK_VARIABLE(key_capslock_table);
   LOCK_VARIABLE(key_shift_table);
   LOCK_VARIABLE(key_control_table);
   LOCK_VARIABLE(key_numlock_table);
   LOCK_VARIABLE(key_extended_table);
   LOCK_VARIABLE(key_special_table);
   LOCK_VARIABLE(key_buffer);
   LOCK_VARIABLE(key_buffer_start);
   LOCK_VARIABLE(key_buffer_end);
   LOCK_VARIABLE(key_extended);
   LOCK_VARIABLE(key_pad_seq);
   LOCK_FUNCTION(my_keyint);

   clear_keybuf();

   /* transfer keys from keyboard buffer */
   while ((kbhit()) && (key_buffer_end < KEY_BUFFER_SIZE-1))
      key_buffer[key_buffer_end++] = getch();

   /* get state info from the BIOS */
   _dosmemgetw(0x417, 1, &shifts);

   key_shifts = 0;

   if (shifts & 1) {
      key_shifts |= KB_SHIFT_FLAG;
      key[KEY_RSHIFT] = TRUE;
   }
   if (shifts & 2) {
      key_shifts |= KB_SHIFT_FLAG;
      key[KEY_LSHIFT] = TRUE;
   }
   if (shifts & 4) {
      key_shifts |= KB_CTRL_FLAG;
      key[KEY_LCONTROL] = TRUE;
   }
   if (shifts & 8) {
      key_shifts |= KB_ALT_FLAG;
      key[KEY_ALT] = TRUE;
   }
   if (shifts & 16)
      key_shifts |= KB_SCROLOCK_FLAG;
   if (shifts & 32)
      key_shifts |= KB_NUMLOCK_FLAG;
   if (shifts & 64)
      key_shifts |= KB_CAPSLOCK_FLAG;


   
   pmint.pm_selector = _my_cs();
   pmint.pm_offset = (unsigned)&my_keyint;
   /*_go32_dpmi_set_protected_mode_interrupt_vector(9, &pmint);*/
   _go32_dpmi_allocate_iret_wrapper(&pmint);
   DISABLE();
   _go32_dpmi_get_protected_mode_interrupt_vector(9, &oldint);
   _go32_dpmi_set_protected_mode_interrupt_vector(9, &pmint);

   update_leds();
   ENABLE();

   /*_add_exit_func(remove_keyboard);*/
   keyboard_installed = TRUE;
   return 0;
}



/* remove_keyboard:
 *  Removes the keyboard handler, returning control to the BIOS. You don't
 *  normally need to call this, because allegro_exit() will do it for you.
 */
static void remove_keyboard()
{
   unsigned short shifts;

   if (!keyboard_installed)
      return;

   /*_remove_irq(KEYBOARD_INT);*/

   /* transfer state info to the BIOS */
   shifts = 0;

   if (key[KEY_RSHIFT])
      shifts |= 1;
   if (key[KEY_LSHIFT])
      shifts |= 2;
   if (key[KEY_LCONTROL])
      shifts |= 4;
   if (key[KEY_ALT])
      shifts |= 8;
   if (key_shifts & KB_SCROLOCK_FLAG)
      shifts |= 16;
   if (key_shifts & KB_NUMLOCK_FLAG)
      shifts |= 32;
   if (key_shifts & KB_CAPSLOCK_FLAG)
      shifts |= 64;

   _dosmemputw(&shifts, 1, 0x417);

   clear_keybuf();

   /*_remove_exit_func(remove_keyboard);*/
   _go32_dpmi_set_protected_mode_interrupt_vector(9, &oldint);
   _go32_dpmi_free_iret_wrapper(&pmint);
   keyboard_installed = FALSE;
}



static int dos_init(aa_context * c, int mode)
{
    install_keyboard();
    return 1;
}
static void dos_uninit(aa_context * c)
{
    remove_keyboard();
}

static int mygetch()
{
    int i = readkey();
    if (!i) return AA_NONE;
    if (i) {
	if (i == 8)
	    return AA_BACKSPACE;
	if (i == 27)
	    return AA_ESC;
	return (i);
    }
    if (!i)
	switch (getch()) {
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
    return (AA_UNKNOWN);
}

static int dos_getchar(aa_context * c1, int wait)
{
    if(c1->mousedriver!=NULL) wait=0;
    if (wait)
	return (mygetch());
    if (keypressed())
	return (mygetch());
    else
	return (AA_NONE);
}


__AA_CONST struct aa_kbddriver kbd_dos_d =
{
    "dos", "dos keyboard driver 2.0",
    AA_SENDRELEASE,
    dos_init,
    dos_uninit,
    dos_getchar,
};
