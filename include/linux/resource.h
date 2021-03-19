#ifndef _LINUX_RESOURCE_H
#define _LINUX_RESOURCE_H

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	(-1)
#define RUSAGE_BOTH	(-2)		/* sys_wait4() uses this */

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* maximum resident set size */
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data size */
	long	ru_isrss;		/* integral unshared stack size */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
};

#define RLIMIT_CPU	0		/* CPU time in ms */
#define RLIMIT_FSIZE	1		/* Maximum filesize */
#define RLIMIT_DATA	2		/* max data size */
#define RLIMIT_STACK	3		/* max stack size */
#define RLIMIT_CORE	4		/* max core file size */
#define RLIMIT_RSS	5		/* max resident set size */

#define RLIM_NLIMITS	6

#define RLIM_INFINITY	0x7FFFFFFF

struct rlimit {
	int	rlim_cur;
	int	rlim_max;
};

#define	PRIO_MIN	(-99)
#define	PRIO_MAX	14

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

#endif