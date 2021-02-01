#ifndef _SYS_TIME_H
#define _SYS_TIME_H

struct timeval {
	long	tv_sec;		/* seconds */		/* 秒 */
	long	tv_usec;	/* microseconds */	/* 微秒 */
};

struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;		/* type of dst correction */
};

#endif