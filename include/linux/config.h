#ifndef _LINUX_CONFIG_H
#define _LINUX_CONFIG_H

#include <linux/autoconf.h>

#ifndef UTS_SYSNAME
#define UTS_SYSNAME "Linux"
#endif

#ifndef UTS_NODENAME
#define UTS_NODENAME "(none)"
#endif

#ifndef UTS_MACHINE
#define UTS_MACHINE "i386"
#endif

#ifndef UTS_DOMAINNAME
#define UTS_DOMAINNAME "(none)"
#endif

#define DEF_INITSEG 0x9000
#define DEF_SYSSEG 0x1000
#define DEF_SETUPSEG 0x9020
#define DEF_SYSSIZE 0x7F00

#define NORMAL_VGA 0xffff
#define EXTENDED_VGA 0xfffe
#define ASK_VGA 0xfffd

#undef HD_TYPE

#endif
