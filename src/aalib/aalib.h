
/* 
 *               AA-lib, an ascii-art gfx library
 *                  Copyright (C) 1998,1999,2001 by
 *
 *      Jan Hubicka          (hubicka@freesoft.cz)
 *      Kamil Toman          (toman@artax.karlin.mff.cuni.cz)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __AALIB_INCLUDED__
#define __AALIB_INCLUDED__
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

#define __AA_CONST const

/* The -malign-double switch changes binarry compatibility with structures
   containing floating point values.  To avoid this, set alignment manually
   to old value.  */

#ifdef __GNUC__
#ifdef __i386__
#define __AA_ALIGN __attribute__ ((__aligned__ (4)))
#define __AA_NOALIGN __attribute__ ((__packed__))
#endif
#endif
#ifndef __AA_ALIGN
#define __AA_ALIGN
#endif
#ifndef __AA_NOALIGN
#define __AA_NOALIGN
#endif

#define AA_LIB_VERSION 1
#define AA_LIB_MINNOR 4
#define AA_LIB_VERSIONCODE 104000

#define AA_NATTRS 5
#define AA_NPARAMS 5

#define AA_RESIZE 258        /*special keycodes */
#define AA_MOUSE 259
#define AA_UP 300
#define AA_DOWN 301
#define AA_LEFT 302
#define AA_RIGHT 303
#define AA_BACKSPACE 304
#define AA_ESC 305
#define AA_UNKNOWN 400
#define AA_RELEASE 65536


#define AA_NORMAL_MASK 1	/*masks for attributes */
#define AA_DIM_MASK 2
#define AA_BOLD_MASK 4
#define AA_BOLDFONT_MASK 8
#define AA_REVERSE_MASK 16
#define AA_ALL 128
#define AA_EIGHT 256
#define AA_EXTENDED (AA_ALL|AA_EIGHT)

/* The characters may be output with following attributes:  */
enum aa_attribute {
AA_NORMAL=0,         /* Normal characters.  */
AA_DIM=1,	     /* Dark characters.  */
AA_BOLD=2,	     /* Bright characters.  */
AA_BOLDFONT=3,	     /* Characters rendered in bold font.  */
AA_REVERSE=4,	     /* Reversed (black on whilte) characters.  */
AA_SPECIAL=5	     /* Render characters in a way easilly visible on the
			screen.  The exact rendering is driver dependent,
			but this mode ought to be used to output texts you
			want to make easilly visible in the image.  */
};
 
enum aa_dithering_mode {
   AA_NONE=0,
   AA_ERRORDISTRIB=1,	/* Error distribution.  */
   AA_FLOYD_S=2,	/* Floyd-Steinberg dithering.  */
   AA_DITHERTYPES=3	/* Number of supported dithering modes.  */
};

#define AA_BUTTON1 1		/*mouse */
#define AA_BUTTON2 2
#define AA_BUTTON3 4
#define AA_MOUSEMOVEMASK 1
#define AA_MOUSEPRESSMASK 2
#define AA_PRESSEDMOVEMASK 4
#define AA_MOUSEALLMASK 7
#define AA_HIDECURSOR 8

#define AA_SENDRELEASE 1
#define AA_KBDALLMASK 1

#define AA_USE_PAGES 1		/*save format flags */
#define AA_NORMAL_SPACES 8

/* These structures are subject to change in future. Thus they are hidden
   behind final user.  */
struct aa_driver;
struct aa_kbddriver;
struct aa_mousedriver;
struct aa_edit;

struct aa_hardware_params {
    __AA_CONST struct aa_font *font;  /*Font bitmap used by hardware.  */
    int supported;		      /*Mask of supported features.
				        Following masks are available:
				        AA_NORMAL_MASK, AA_DIM_MASK,
					AA_BOLD_MASK, AA_BOLDFONT_MASK,
				        AA_REVERSE_MASK
				       */
    int minwidth, minheight;	      /*Minimal alowed screen size 
					in characters  */
    int maxwidth, maxheight;	      /*Maximal alowed screen size 
					in characters  */
    int recwidth, recheight;	      /*Recommended screen size (driver
					attempts to find resolution nearest
					to this value within given bounds
					bit min and max fields  */
    int mmwidth, mmheight;	      /*Physical screen size  */
    int width, height;		      /*Current screen size  */
    double dimmul __AA_ALIGN;	      /*Bright value of dim characters (0 black,
					1 white)  */
    double boldmul __AA_ALIGN;	      /*Bright value of bold characters 
					(0 black, 1 white)  */
};

/*
 * aa_context contains information about the display. It is passed to most
 * of AA-lib functions.  The full definition of structure is present just
 * for compatibility with older programs. Dirrect access to it's fields
 * is not recommended, because you might surfer to problems with future
 * releases of AA-lib.  Use standard AA-lib functions instead.  
 */
struct aa_context {
    __AA_CONST struct aa_driver *driver;  /*Current display driver  */
    __AA_CONST struct aa_kbddriver *kbddriver;  /*Current keyboard driver */
    __AA_CONST struct aa_mousedriver *mousedriver; /*Current mouse driver */
    struct aa_hardware_params __AA_NOALIGN params; /*Parameters of output
						     hardware used by AA-lib. */
    struct aa_hardware_params __AA_NOALIGN driverparams; /*Parameters of output
							   hardware as reported
							   by display driver. */
    int mulx, muly;		/* Ratio of character size over pixel size  */
    int imgwidth, imgheight;    /* Dimensions of emulated image  */
    unsigned char *imagebuffer; /* Virtual buffer containing image */
    unsigned char *textbuffer;  /* Virtual buffer containing text */
    unsigned char *attrbuffer;  /* Virtual buffer containing attributes */
    unsigned short *table;      /* Precalculated values used by rendering
				   algorithm.
				   
				   WARNING!
				   It is _strongly_ depreached
				   to access fields behind this point, because
				   they can change in future releases.*/
    unsigned short *filltable;  
    struct parameters *parameters;
    int cursorx, cursory, cursorstate;  /* Cursor possition. */
    int mousex, mousey, buttons,mousemode; /* Mouse state.  */
    void (*resizehandler) (struct aa_context *); /* Handler to be called when
						    resize happends.  */
    void *driverdata;		/* Internal data used by hardware drivers.  */
    void *kbddriverdata;
    void *mousedriverdata;
};


/*
 * The hardware driver specification.  Used internally by AA-lib only.
 * Provided for compatibility with older programs.
 */
struct aa_driver {
    __AA_CONST char *shortname, *name;
    int (*init) (__AA_CONST struct aa_hardware_params *, __AA_CONST void *, struct aa_hardware_params *,void **);
    void (*uninit) (struct aa_context *);
    void (*getsize) (struct aa_context *, int *, int *);
    void (*setattr) (struct aa_context *, int);
    void (*print) (struct aa_context *, __AA_CONST char *);
    void (*gotoxy) (struct aa_context *, int, int);
    void (*flush) (struct aa_context *);
    void (*cursormode) (struct aa_context *, int);
};

/*
 * The hardware driver specification.  Used internally by AA-lib only.
 * Provided for compatibility with older programs.
 */
struct aa_kbddriver {
    __AA_CONST char *shortname, *name;
    int flags;
    int (*init) (struct aa_context *, int mode);
    void (*uninit) (struct aa_context *);
    int (*getkey) (struct aa_context *, int);
};

/*
 * The hardware driver specification.  Used internally by AA-lib only.
 * Provided for compatibility with older programs.
 */
struct aa_mousedriver {
    __AA_CONST char *shortname, *name;
    int flags;
    int (*init) (struct aa_context *, int mode);
    void (*uninit) (struct aa_context *);
    void (*getmouse) (struct aa_context *, int *, int *, int *);
    void (*cursormode) (struct aa_context *,int);
};
/*
 * Parameters used for rendering.
 */
struct aa_renderparams {
    int bright;                     /* Brighness in range 0 (normal) to 255
				       (white)  */
    int contrast;                   /* Contrast value in range 0 (normal)
				       to 127 (white)  */
    float gamma __AA_ALIGN;         /* Gama value in the standard range  */
    enum aa_dithering_mode dither;  /* Dithering algorithm */
    int inversion;                  /* Set 1 for inversed terminals (black on
				       white)  */
    int randomval;                  /* Range of random value added to each
				       pixel. Used to crate random dithering
				       effect.  */
};


struct aa_font {
    __AA_CONST unsigned char *data;  /* Bitmap of 8xheight font.  */
    int height;	                     /* Height of font.  */
    __AA_CONST char *name;	     /* Long name of font.  */
    __AA_CONST char *shortname;	     /* One-word name of the font.  */
};

/*
 * Output format specification used by aa_save driver. 
 */
struct aa_format {
    int width, height;
    int pagewidth, pageheight;
    int flags;
    int supported;
    __AA_CONST struct aa_font *font;
    __AA_CONST char *formatname;
    __AA_CONST char *extension;
    /*fields after this line may change in future versions*/
    __AA_CONST char *head;
    __AA_CONST char *end;
    __AA_CONST char *newline;
    __AA_CONST char * __AA_CONST prints[AA_NATTRS];
    __AA_CONST char * __AA_CONST begin[AA_NATTRS];
    __AA_CONST char * __AA_CONST ends[AA_NATTRS];
    __AA_CONST char * __AA_CONST *conversions;	
};

/*
 * Initialization data used by aa_save driver. 
 */
struct aa_savedata {
    char *name; /* Base name of the output file. The page number and extension
		   is attached automatically.  */
    __AA_CONST struct aa_format *format; /* Format to save into.  */
    FILE *file; /* You might specify output file by field too,
		   in case the name field is NULL*/
};

typedef struct aa_context aa_context;
typedef struct aa_linkedlist aa_linkedlist;
typedef struct aa_linkedlist aa_reclist;
typedef struct aa_renderparams aa_renderparams;
typedef struct aa_hardware_params aa_hardwareparams;
typedef struct aa_driver aa_driver;
typedef struct aa_font aa_font;
typedef struct aa_format aa_format;
typedef struct aa_savedata aa_savedata;
typedef int aa_palette[256];


/*
 * AA-lib driver used to save ascii-art image into file in used specified format. 
 * Initialize this driver using aa_init function and specify the driver
 * dependent parameters in aa_savedata structure to save image into file.
 * See the texinfo documentation for details.
 */
extern __AA_CONST struct aa_driver save_d; 

/*
 * AA-lib memory driver.  
 * Used to render ascii-art images into memory. 
 * You might use this driver to render images into memory and then use your
 * own routines to handle them in case you want to avoid AA-lib's output
 * mechanizms.
 */
extern __AA_CONST struct aa_driver mem_d;

/*
 * AA-lib help string for the default command line parser.
 */
extern __AA_CONST char * __AA_CONST aa_help;

/*
 * NULL terminated array of save formats supported by AA-lib.
 */
extern __AA_CONST struct aa_format * __AA_CONST aa_formats[];
extern __AA_CONST struct aa_format aa_nhtml_format, aa_html_format,
        aa_html_alt_format, aa_ansi_format, aa_text_format, aa_more_format,
	aa_hp_format, aa_hp2_format, aa_irc_format, aa_zephyr_format,
	aa_htmlk_format;

/*
 * Null-terminated array of available fonts.
 */
extern __AA_CONST struct aa_font *aa_fonts[];
extern __AA_CONST struct aa_font aa_font8, aa_font14, aa_font16, aa_font9,
	aa_fontline, aa_fontgl, aa_fontX13, aa_fontX16, aa_fontX13B,
	aa_fontcourier, aa_fontvyhen;
/*
 * Names of dithering methods supported by AA-lib.
 * NULL terminated array containing the names of supported dithering methods
 * as ascii strings.
 */
extern __AA_CONST char * __AA_CONST aa_dithernames[];

/*
 * NULL-terminated array of output drivers available in AA-lib.
 */
extern __AA_CONST struct aa_driver * __AA_CONST aa_drivers[];
extern __AA_CONST struct aa_driver curses_d, dos_d, linux_d, slang_d, stdout_d,
				   stderr_d, X11_d, os2vio_d;

/*
 * NULL-terminated array of keyboard drivers available in AA_lib.
 */
extern __AA_CONST struct aa_kbddriver * __AA_CONST aa_kbddrivers[];
extern __AA_CONST struct aa_kbddriver kbd_curses_d, kbd_slang_d, kbd_stdin_d,
			              kbd_dos_d, kbd_X11_d, kbd_os2_d,
				      kbd_linux_d;

/*
 * NULL terminated array of mouse drivers supported by AA-lib.
 */
extern __AA_CONST struct aa_mousedriver * __AA_CONST aa_mousedrivers[];
extern __AA_CONST struct aa_mousedriver mouse_curses_d, mouse_gpm_d,
					mouse_X11_d, mouse_dos_d, mouse_os2_d;

/*
 * List of recommended drivers.
 * List of recommended drivers is used by aa_autoinit familly of functions
 * and altered by aa_recommend familly of functions.
 */
extern aa_linkedlist *aa_kbdrecommended, *aa_mouserecommended, *aa_displayrecommended;

/*
 * default hardware paramters requested by AA-lib programs. Pa
 * Default hardware paramters requested by AA-lib programs. Passed to aa_init
 * function familly.
 */
extern struct aa_hardware_params aa_defparams;

/*
 * default rendering parameters.
 * Default rendering parameters. Passed to aa_render function familly.
 */
extern struct aa_renderparams aa_defrenderparams;


/* returns width of the output screen in characters.  */
int aa_scrwidth(
		/* Specifies the AA-lib context to operate on. */
		aa_context *a);
/* returns height of the output screen in characters.  */
int aa_scrheight(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/* returns width of the output screen in millimeters.  */
int aa_mmwidth(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/* returns height of the output screen in millimeters.  */
int aa_mmheight(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/* returns width of the emulated image in pixels.  */
int aa_imgwidth(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/* returns height of the emulated image in pixels.  */
int aa_imgheight(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/*
 * returns pointer to the framebuffer emulated by AA-lib.
 * The framebuffer is simple array of characters specifying
 * the brightness value (or palette index depending on the aa_render
 * call). The array is organizated in the aa_imgheight (a) rows of
 * aa_imgwidth(a) characters.
 * Returns pointer to framebuffer emulated by AA-lib.
 */
char *aa_image(
		 /* Specifies the AA-lib context to operate on. */
		 aa_context *a);
/*
 * returns pointer to the text output buffer used by AA-lib.
 * The text output buffer is simple array of characters specifying
 * the ascii-value of the characters.
 * The array is organizated in the aa_scrheight (a) rows of
 * aa_scrwidth(a) characters.
 *
 * Returns pointer the text output buffer used by AA-lib.
 */
char *aa_text(
	      /* Specifies the AA-lib context to operate on. */
	      aa_context *a);
/*
 * returns pointer to the text output buffer used by AA-lib.
 * The attribute output buffer is simple array of characters specifying
 * the attributes.
 * The array is organizated in the aa_scrheight (a) rows of
 * aa_scrwidth(a) characters.
 *
 * Returns pointer the text output buffer used by AA-lib.
 */
char *aa_attrs(
	       /* Specifies the AA-lib context to operate on. */
	       aa_context *a);

/*
 * returns specification of the fonts used by AA-lib rendering routines.
 */
__AA_CONST struct aa_font *aa_currentfont(
	       /* Specifies the AA-lib context to operate on. */
	       aa_context *a);

/*
 * easy to use AA-lib initialization function. 
 * Attempts to find available output driver supporting the specified
 * parameters.  First attempts to initialize the recommended drivers
 * and then in order drivers available in the aa_drivers array
 * (all regular output drivers compiled into AA-lib).
 * Returns pointer to initialized context structure when succesfull or
 * NULL on failure.
 */
aa_context *aa_autoinit(
		        /* Hardware parameters you want.  Use aa_defparams
		           for default values.  */
		        __AA_CONST struct aa_hardware_params *params);
/*
 * easy to use AA-lib keyboard initialization function. 
 * Attempts to find available keyboard driver supporting the specified
 * mode.  First attempts to initialize the recommended drivers
 * and then in order drivers available in the aa_kbddrivers array
 * (all regular output drivers compiled into AA-lib).
 *
 * Every AA-lib program ought to have call to aa_parseoptions before
 * first call to aa_init.
 *
 * Returns 1 when succesfull or 0 on failure.
 */
int aa_autoinitkbd(
	           /* Specifies the AA-lib context to operate on. */
		   struct aa_context *context,
	           /* Mask of extra features you request. Can contain
		      AA_SENDRELEASE if you are interested in release events
		      too. */
		   int mode);
/*
 * easy to use AA-lib mouse initialization function. 
 * Attempts to find available mouse driver supporting the specified
 * mode.  First attempts to initialize the recommended drivers
 * and then in order drivers available in the aa_kbddrivers array
 * (all regular output drivers compiled into AA-lib).
 * Returns 1 when succesfull or 0 on failure.
 */
int aa_autoinitmouse(
	             /* Specifies the AA-lib context to operate on. */
		     struct aa_context *c,
		     /* Mask of extra features you request.  No such features
		        are available in the current AA-lib version.  */
		     int mode);

/* insert the given driver on beggining of the list of recommended drivers.  */
void aa_recommendhi(
		    /* List to operate on (aa_displayrecommended,
		       aa_kbdrecommended or aa_mouserecommended) */
		    aa_linkedlist ** l,
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);

/* Add the given driver to the end of list of recommended drivers.  */
void aa_recommendlow(
		    /* List to operate on (aa_displayrecommended,
		       aa_kbdrecommended or aa_mouserecommended) */
		    aa_linkedlist ** l,
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);
char *aa_getfirst(aa_linkedlist ** l);

/*init functions */

/*
 * open the output display for AA-lib. 
 * This is the most primitive AA-lib initialization function.
 * Allows better control over the process than the easier to use
 * aa_autoinit function.
 *
 * Every AA-lib program ought to have call to aa_parseoptions before
 * first call to aa_init.
 *
 * returns pointer to new AA-lib context or NULL if failed.  
 */

aa_context *aa_init(
		      /* Driver you want to use.  Available drivers are listed
		         in the NULL terminated aa_drivers array.  */
		    __AA_CONST struct aa_driver *driver,  
		      /* Hardware parameters you want.  Use aa_defparams
		         for default values.  */
		    __AA_CONST struct aa_hardware_params *defparams, 
		      /* This pointer is passed dirrectly to driver used
		         to specify additional driver dependent parameters. */
		    __AA_CONST void *driverdata);

/*
 * initialize the AA-lib keyboard driver. 
 * This is the most primitive AA-lib keyboard initialization function.
 * Allows better control over the process than the easier to use
 * aa_autoinitkbd function.
 * returns 1 on success and 0 on fail.  
 */

int aa_initkbd(
	       /* Specifies the AA-lib context to operate on.  */
	       struct aa_context *context,  
	       /* Driver you wish to use */
	       __AA_CONST struct aa_kbddriver *drv, 
	       /* Mask of extra features you request. Can contain
		  AA_SENDRELEASE if you are interested in release events
		  too. */
	       int mode);
/*
 * initialize the AA-lib mouse driver. 
 * This is the most primitive AA-lib keyboard initialization function.
 * Allows better control over the process than the easier to use
 * aa_autoinitmouse function.
 * returns 1 on success and 0 on fail.  
 */

int aa_initmouse(
		   /* Specifies the AA-lib context to operate on.  */
		 struct aa_context *c, 
		   /* Driver you wish to use.  */
		__AA_CONST struct aa_mousedriver *d, 
		   /* Mask of extra features you request.  No such features
		      are available in the current AA-lib version.  */
		 int mode);

/*uninicializing functions */

/*
 * close the AA-lib context.
 * Uninitialize all activated drivers and frees the memory used by context
 * structures.
 */
void aa_close(aa_context * c /* Specifies the AA-lib context to operate on.  */
	      );
/*
 * uninitialize the keyboard driver. 
 * Calls "uninitialize" function of the keyboard driver. It ought to undo
 * all actions done by "initialize" function.
 */
void aa_uninitkbd(
		  /* Specifies the AA-lib context to operate on.  */
		  aa_context *context);
/*
 * uninitialize the mouse driver. 
 * Calls "uninitialize" function of the mouse driver. It ought to undo
 * all actions done by "initialize" function.
 */
void aa_uninitmouse(
		    /* Specifies the AA-lib context to operate on.  */
		    aa_context *context);

/* simple and fast AA-lib rendering function.
 * This function does the trick of converting the emulated framebuffer
 * into high quality ASCII-art. It is slightly faster and less flexible
 * the aa_render function.
 *
 * Note that to see the effect you need to call aa_flush too.
 *
 * First call to this function may take a while, because the rendering
 * tables are produced.
 */
void aa_fastrender(
		   /* Specifies the AA-lib context to operate on.  */
		   aa_context * c, 
		   /* column of top left coner of rendered area
		      (in characters!) */
		   int x1,
		   /* row of top left coner of rendered area */
		   int y1,
		   /* column of bottom right coner of rendered area */
		   int x2,
		   /* row of bottom right coner of rendered area */
		   int y2);
/* convert image buffer to ASCII-art.
 * This function does the trick of converting the emulated framebuffer
 * into high quality ASCII-art. If you want to be really fast, you might
 * use aa_fastrender.  If you want to emulate palette, use aa_renderpalette.
 *
 * Note that to see the effect you need to call aa_flush too.
 *
 * First call to this function may take a while, because the rendering
 * tables are produced.
 */
void aa_render(
	       /* Specifies the AA-lib context to operate on.  */
	       aa_context * c,
	       /* Rendering parametters used to specify brightness, gamma
		  correction and other usefull stuff. Use aa_defrenderparams
		  for default values. */
	       __AA_CONST aa_renderparams * p, 
	       /* column of top left coner of rendered area
	          (in characters!) */
	       int x1,
	       /* row of top left coner of rendered area */
	       int y1,
	       /* column of bottom right coner of rendered area */
	       int x2,
	       /* row of bottom right coner of rendered area */
	       int y2);

void aa_renderpalette(aa_context * c, __AA_CONST aa_palette table,
	       __AA_CONST aa_renderparams * p, int x1, int y1, int x2, int y2);
aa_renderparams *aa_getrenderparams(void);
void aa_flush(aa_context * c);

/* output string to AA-lib output buffers.
   Output given string to AA-lib output buffers.  To see the effect you need to
   call aa_flush too.  */
void aa_puts(
	     /* Specifies the AA-lib context to operate on.  */
	     aa_context * c,
	     /* X coordinate of the first character.  */
	     int x,
	     /* Y coordinate of the first character.  */
	     int y,
	     /* Attribute to use.  */
	     enum aa_attribute attr,
	     /* String to output.  */
	     __AA_CONST char *s);
/*
 * print text to AA-lib output buffers.
 * Print given text to AA-lib output buffers.  To see the effect you need to
 * call aa_flush too.  
 */
int aa_printf(
	     /* Specifies the AA-lib context to operate on.  */
	     aa_context * c,
	     /* X coordinate of the first character.  */
	     int x,
	     /* Y coordinate of the first character.  */
	     int y,
	     /* Attribute to use.  */
	     enum aa_attribute attr,
	     /* Text to output in standard printf format.  */
	      __AA_CONST char *fmt, ...);
/*
 * move the hardware cursor (if any) to specified position. 
 * Move the hardware cursor (if any) to specified position. 
 * To see the effect you need to call aa_flush too.  
 */
void aa_gotoxy(
	     /* Specifies the AA-lib context to operate on.  */
	       aa_context * c,
	     /* X coordinate of new position.  */
	     int x,
	     /* Y coordinate of the position.  */
	     int y);
/*
 * hide the hardware cursor.
 * Hide the hardware cursor.
 * This function may be ignored by some drivers.
 */
void aa_hidecursor(
	           /* Specifies the AA-lib context to operate on.  */
	           aa_context * c);
/*
 * show the hardware cursor.
 * Show the hardware cursor.
 * This function may not be ignored by some drivers.
 */
void aa_showcursor(
	           /* Specifies the AA-lib context to operate on.  */
	           aa_context * c);
/*
 * Get mouse position as specified by last mouse event read by aa_getevent.
 */
void aa_getmouse( 
	         /* Specifies the AA-lib context to operate on.  */
		 aa_context * c,
		 /* Used to return X coordinate of mouse in characters.  */
		 int *x,
		 /* Used to return Y coordinate of mouse in characters.  */
		 int *y,
		 /* Used to return button mask of mouse. 
		    (values used are AA_BUTTON1, AA_BUTTON2 and AA_BUTTON3)*/
		 int *b);
/*
 * hide the mouse cursor.
 * Hide the mouse cursor. 
 * This function may be ignored by some drivers.
 */
void aa_hidemouse(aa_context *c);
/*
 * show the mouse cursor.
 * Show the mouse cursor. 
 * This function may be ignored by some drivers.
 */
void aa_showmouse(aa_context *c);

/*
 * add new font specification to aa_fonts array.
 * Returns 1 when succesfull or 0 on failure.
 */
int aa_registerfont(
		    /* Font specification structure. */
		    __AA_CONST struct aa_font *f);
/*
 * alter the "supported" field of hardware_params structure used by AA-lib
 * This function can be used to alter "supported" field of hardware-params
 * structure used by AA-lib. 
 */
void aa_setsupported(
	             /* Specifies the AA-lib context to operate on.  */
		     aa_context * c,
		     /* New mask of requested features. Can contain
		        AA_EXTENDED to enable use of all 256 characters
		        and AA_EIGHT to enable use of the character numbered
		        higher than 127.  */
		     int supported);
/* set font specification to be used by rendering functions. */
void aa_setfont(
                /* Specifies the AA-lib context to operate on.  */
		aa_context * c,
		/* Font specification structure.  */
		__AA_CONST struct aa_font *font);

/*keyboard functions */
/* return next event from queue.
 * Return next even from queue. Optionally wait for even when queue is
 * empty.
 * Returns next event from queue (values lower than 256 are used to report
 * ascii values of pressed keys and higher values have special meanings)
 * See the AA-lib texinfo documentation for more details.
 * 0 is returned when queue is empty and wait is set to 0.
 */
int aa_getevent(
                /* Specifies the AA-lib context to operate on.  */
		aa_context * c,
		/* 1 if you wish to wait for the even when queue is empty.  */
		int wait);
/* return next keypress event from queue.
 * Returns next keypress event from queue (values lower than 256 are used
 * to report ascii values of pressed keys and higher values are used to
 * represent some special keys like arrows)
 * See the AA-lib texinfo documentation for more details.
 */
int aa_getkey(
                /* Specifies the AA-lib context to operate on.  */
		aa_context * c,
		/* 1 if you wish to wait for the even when queue is empty.  */
		int wait);

/*resize functions */
/* 
 * Do resize action. This function ought to be called when application
 * takes into account the AA_RESIZE event.  The context is reinitialized
 * and set to new sizes.
 * Returns 0 when no resizing is done and 1 when resizing was succesfull.
 */
int aa_resize(
                /* Specifies the AA-lib context to operate on.  */
		aa_context * c);
/*
 * Set user handler to be called on resize event.
 */
void aa_resizehandler(
                /* Specifies the AA-lib context to operate on.  */
		aa_context * c,
		/* Function to be called when resize happends.  */
		void (*handler) (aa_context *));

/*
 * parse the standard command line options used by AA-lib.
 * Use this function to parse the standard command line options used by
 * AA-lib. Every AA-lib program ought to call this function to let user
 * specify some extra parameters.  The function alters the aa_hardware_params
 * and aa_renderparams structures and removes known options from the
 * argc/argv lists. It also parse the AAOPTS environment variable.
 * When called with NULL for the argc/argv parameters, it parses AAOPTS
 * only. At least this call ought to be in every AA-lib program.
 * Returns 1 when sucesfull and 0 on failure. The program then can
 * print the help text available in aa_help variable.
 */
int aa_parseoptions(
		    /* Hardware parameters structure to alter. It is expected
		     * that this structure only with necessary modifications
		     * will be later used to initialize the AA-lib context.  */
		    struct aa_hardware_params *p,
		    /* Rendering prameters structure to alter. It is expected
		     * that this structure only with necessary modifications
		     * will be later used to render images.  */
		    aa_renderparams * r,
		    /* Pointer to argc parameter passed to function "main".  */
		    int *argc,
		    /* Pointer to argv parameter passed to function "main".  */
		    char **argv);

/*
 * Simple interactive line editor.
 * This function produces the simple interactive line editor that can
 * be used by AA-lib programs to input strings.
 */
void aa_edit(
             /* Specifies the AA-lib context to operate on.  */
	     aa_context * c, 
	     /* X coordinate of the edited text.  */
	     int x,
	     /* Y coordinate of the edited text.  */
	     int y,
	     /* Length of the editor window.  */
	     int size,
	     /* Buffer to edit (containing default value).  */
	     char *s,
	     /* Size of the buffer.  */
	     int maxsize);
/*
 * Simple interactive line editor provided as helper function.
 *
 * You might use this function to input strings in AA-lib programs.
 * This function initializes the aa_edit structure used by event-based
 * editor. You might then call the aa_editkey function when key is pressed.
 *
 * Returns pointer to edit context when succesfull and NULL on failure.
 */
struct aa_edit *aa_createedit(
             /* Specifies the AA-lib context to operate on.  */
	     aa_context * c, 
	     /* X coordinate of the edited text.  */
	     int x,
	     /* Y coordinate of the edited text.  */
	     int y,
	     /* Length of the editor window.  */
	     int size,
	     /* Buffer to edit (containing default value).  */
	     char *s,
	     /* Size of the buffer.  */
	     int maxsize);

/* Notify the line editor about keypress. */
void aa_editkey(
		/* Editor context to use (see aa_createedit) */
		struct aa_edit *e,
		/* Key pressed  */
		int c);

/*
 * put pixel to emulated framebuffer
 */
void aa_putpixel(
                 /* Specifies the AA-lib context to operate on.  */
	         aa_context * c, 
		 /* X coordinate. */
		 int x,
		 /* Y coordinate. */
		 int y,
		 /* Palette index or brightness value (0..255) */
		 int color);

/* insert the given driver on beggining of the list of recommended keyboard drivers.  */
void aa_recommendhikbd(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);

/* Add the given driver to the end of list of keyboard recommended drivers.  */
void aa_recommendlowkbd(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);
/* insert the given driver on beggining of the list of recommended mouse drivers.  */
void aa_recommendhimouse(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);

/* Add the given driver to the end of list of mouse recommended drivers.  */
void aa_recommendlowmouse(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);
/* insert the given driver on beggining of the list of recommended display drivers.  */
void aa_recommendhidisplay(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);

/* Add the given driver to the end of list of display recommended drivers.  */
void aa_recommendlowdisplay(
		    /* Name of the driver (ought to match the "shortname"
		       field of the driver definition structure).  */
		    __AA_CONST char *name);



/* This macro implementations are proved for faster compilation. */
#ifdef __GNUC__
/* The putpixel macro can be implemented reliably only using GNU-C extension.  */
#define aa_putpixel(c,x,y,color) ({aa_context *___aa_context=(c);  ((___aa_context)->imagebuffer[(x)+(y)*(aa_imgwidth(___aa_context))]=(color)); 0;})
#endif
#define aa_setpalette(palette,index,r,g,b) ((palette)[index]=(((r)*30+(g)*59+(b)*11)>>8))
#define aa_recommendhikbd(t) aa_recommendhi(&aa_kbdrecommended,t);
#define aa_recommendhimouse(t) aa_recommendhi(&aa_mouserecommended,t);
#define aa_recommendhidisplay(t) aa_recommendhi(&aa_displayrecommended,t);
#define aa_recommendlowkbd(t) aa_recommendlow(&aa_kbdrecommended,t);
#define aa_recommendlowmouse(t) aa_recommendlow(&aa_mouserecommended,t);
#define aa_recommendlowdisplay(t) aa_recommendlow(&aa_displayrecommended,t);
#define aa_scrwidth(a) ((a)->params.width)
#define aa_scrheight(a) ((a)->params.height)
#define aa_mmwidth(a) ((a)->params.mmwidth)
#define aa_mmheight(a) ((a)->params.mmheight)
#define aa_imgwidth(a) ((a)->imgwidth)
#define aa_imgheight(a) ((a)->imgheight)
#define aa_image(c) ((c)->imagebuffer)
#define aa_text(c) ((c)->textbuffer)
#define aa_attrs(c) ((c)->attrbuffer)


#ifdef __cplusplus
}
#endif
#endif
