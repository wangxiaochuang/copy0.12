#ifndef _VT_KERN_H
#define _VT_KERN_H

#include <linux/vt.h>

static struct vt_struct {
	unsigned char	vc_mode;		/* KD_TEXT, ... */
	unsigned char	vc_kbdraw;
	unsigned char	vc_kbde0;
	unsigned char   vc_kbdleds;
	struct vt_mode	vt_mode;
	int		vt_pid;
	int		vt_newvt;
} vt_cons[NR_CONSOLES];

#endif