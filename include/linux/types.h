#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef int ssize_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

typedef int pid_t;
typedef unsigned short uid_t;
typedef unsigned short gid_t;
typedef unsigned short dev_t;
#ifdef OLD_LINUX
typedef unsigned short ino_t;
#else
typedef unsigned long ino_t;
#endif
typedef unsigned short mode_t;
typedef unsigned short umode_t;
typedef unsigned short nlink_t;
typedef int daddr_t;
typedef long off_t;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned long tcflag_t;

#undef __FDSET_LONGS
#define __FDSET_LONGS 8

typedef struct fd_set {
	unsigned long fds_bits [__FDSET_LONGS];
} fd_set;

#undef __NFDBITS
#define __NFDBITS	(8 * sizeof(unsigned long))

#undef __FD_SETSIZE
#define __FD_SETSIZE	(__FDSET_LONGS*__NFDBITS)

#undef __FD_SET
#define __FD_SET(fd, fdsetp) \
	__asm__ __volatile__("btsl %1, %0": \
		"=m" (*(fd_set *) (fdsetp)):"r" ((int) (fd)))

#undef	__FD_CLR
#define __FD_CLR(fd,fdsetp) \
		__asm__ __volatile__("btrl %1,%0": \
			"=m" (*(fd_set *) (fdsetp)):"r" ((int) (fd)))

#undef	__FD_ISSET
#define __FD_ISSET(fd,fdsetp) (__extension__ ({ \
		unsigned char __result; \
		__asm__ __volatile__("btl %1,%2 ; setb %0" \
			:"=q" (__result) :"r" ((int) (fd)), \
			"m" (*(fd_set *) (fdsetp))); \
		__result; }))

#undef	__FD_ZERO
#define __FD_ZERO(fdsetp) \
		__asm__ __volatile__("cld ; rep ; stosl" \
			:"=m" (*(fd_set *) (fdsetp)) \
			:"a" (0), "c" (__FDSET_LONGS), \
			"D" ((fd_set *) (fdsetp)))

struct ustat {
	daddr_t f_tfree;
	ino_t f_tinode;
	char f_fname[6];
	char f_fpack[6];
};
#endif