#ifndef _LINUX_RESOURCE_H
#define _LINUX_RESOURCE_H

#define RLIM_NLIMITS	6

#define RLIM_INFINITY	0x7FFFFFFF

struct rlimit {
	int	rlim_cur;
	int	rlim_max;
};

#endif