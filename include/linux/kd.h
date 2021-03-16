#ifndef _LINUX_KD_H
#define _LINUX_KD_H

#define KDSETMODE	0x4B3A	/* set text/grahics mode */
#define		KD_TEXT		0x00
#define		KD_GRAPHICS	0x01
#define		KD_TEXT0	0x02	/* ? */
#define		KD_TEXT1	0x03	/* ? */
#define KDGETMODE	0x4B3B

struct kbdiacr {
        u_char diacr, base, result;
};

#endif