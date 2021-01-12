#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

typedef unsigned int sigset_t;

struct sigaction {
	void (*sa_handler)(int);	/* 对应某信号指定要采取的行动 */
	sigset_t sa_mask;			/* 对信号的屏蔽码，在信号程序执行时将阻塞对这些信号的处理 */
	int sa_flags;				/* 改变信号处理过程的信号集 */
	void (*sa_restorer)(void);/* 恢复函数指针，由函数库Libc提供，用于清理用户态堆栈 */
};

#endif