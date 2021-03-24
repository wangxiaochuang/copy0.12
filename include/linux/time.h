#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

struct timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* microseconds */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};

#define NFDBITS			__NFDBIT

#define FD_CLR(fd,fdsetp)	__FD_CLR(fd,fdsetp)

#endif