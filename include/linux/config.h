#ifndef _CONFIG_H
#define _CONFIG_H

/* 定义了uname()函数的返回值 */
#define UTS_SYSNAME "Linux"
#define UTS_NODENAME "(none)"   /* set by sethostname() */
#define UTS_RELEASE "0"		    /* patchlevel */
#define UTS_VERSION "0.12"
#define UTS_MACHINE "i386"      /* hardware type */

#define DEF_INITSEG	    0x9000
#define DEF_SYSSEG	    0x1000
#define DEF_SETUPSEG	0x9020

#endif