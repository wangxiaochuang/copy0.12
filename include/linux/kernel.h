#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#ifdef __KERNEL__

#include <linux/linkage.h>

#define INT_MAX		((int)(~0U>>1))
#define UINT_MAX	(~0U)
#define LONG_MAX	((long)(~0UL>>1))
#define ULONG_MAX	(~0UL)

#define	KERN_EMERG	"<0>"	/* system is unusable			*/
#define	KERN_ALERT	"<1>"	/* action must be taken immediately	*/
#define	KERN_CRIT	"<2>"	/* critical conditions			*/
#define	KERN_ERR	"<3>"	/* error conditions			*/
#define	KERN_WARNING	"<4>"	/* warning conditions			*/
#define	KERN_NOTICE	"<5>"	/* normal but significant condition	*/
#define	KERN_INFO	"<6>"	/* informational			*/
#define	KERN_DEBUG	"<7>"	/* debug-level messages			*/

#define NORET_TYPE /**/
# define ATTRIB_NORET  __attribute__((noreturn))
#define NORET_AND     noreturn,

extern void math_error(void);
NORET_TYPE void panic(const char * fmt, ...)
	__attribute__ ((NORET_AND format (printf, 1, 2)));
asmlinkage int printk(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));

#define suser() (current->euid == 0)
#endif

#endif