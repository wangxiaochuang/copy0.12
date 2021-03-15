#ifndef _KBD_KERN_H
#define _KBD_KERN_H

#include <linux/interrupt.h>

#include <linux/keyboard.h>

struct kbd_struct {
        unsigned char ledstate;		/* 3 bits */
	unsigned char default_ledstate;
#define VC_SCROLLOCK	0	/* scroll-lock mode */
#define VC_NUMLOCK	1	/* numeric lock mode */
#define VC_CAPSLOCK	2	/* capslock mode */

	unsigned char lockstate;	/* 4 bits - must be in 0..15 */
#define VC_SHIFTLOCK	KG_SHIFT	/* shift lock mode */
#define VC_ALTGRLOCK	KG_ALTGR	/* altgr lock mode */
#define VC_CTRLLOCK	KG_CTRL 	/* control lock mode */
#define VC_ALTLOCK	KG_ALT  	/* alt lock mode */

	unsigned char modeflags;
#define VC_APPLIC	0	/* application key mode */
#define VC_CKMODE	1	/* cursor key mode */
#define VC_REPEAT	2	/* keyboard repeat */
#define VC_CRLF		3	/* 0 - enter sends CR, 1 - enter sends CRLF */
#define VC_META		4	/* 0 - meta, 1 - meta=prefix with ESC */
#define VC_PAUSE	5	/* pause key pressed - unused */
#define VC_RAW		6	/* raw (scancode) mode */
#define VC_MEDIUMRAW	7	/* medium raw (keycode) mode */
};

extern unsigned long kbd_init(unsigned long);

#endif