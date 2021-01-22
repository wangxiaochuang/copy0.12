#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>

typedef unsigned int sigset_t;

#define SIGHUP		 1				/* Hang Up	-- 挂起控制终端或进程 */
#define SIGINT		 2				/* Interrupt -- 来自键盘的中断 */
#define SIGQUIT		 3				/* Quit		-- 来自键盘的退出 */
#define SIGILL		 4				/* Illeagle	-- 非法指令 */
#define SIGTRAP		 5				/* Trap 	-- 跟踪断点 */
#define SIGABRT		 6				/* Abort	-- 异常结束 */
#define SIGIOT		 6				/* IO Trap	-- 同上 */
#define SIGUNUSED	 7				/* Unused	-- 没有使用 */
#define SIGFPE		 8				/* FPE		-- 协处理器出错 */
#define SIGKILL		 9				/* Kill		-- 强迫进程终止 */
#define SIGUSR1		10				/* User1	-- 用户信号 1，进程可使用 */
#define SIGSEGV		11				/* Segment Violation -- 无效内存引用 */
#define SIGUSR2		12				/* User2    -- 用户信号 2，进程可使用 */
#define SIGPIPE		13				/* Pipe		-- 管道写出错，无读者 */
#define SIGALRM		14				/* Alarm	-- 实时定时器报警 */
#define SIGTERM		15				/* Terminate -- 进程终止 */
#define SIGSTKFLT	16				/* Stack Fault -- 栈出错（协处理器） */
#define SIGCHLD		17				/* Child	-- 子进程停止或被终止 */
#define SIGCONT		18				/* Continue	-- 恢复进程继续执行 */
#define SIGSTOP		19				/* Stop		-- 停止进程的执行 */
#define SIGTSTP		20				/* TTY Stop	-- tty 发出停止进程，可忽略 */
#define SIGTTIN		21				/* TTY In	-- 后台进程请求输入 */
#define SIGTTOU		22				/* TTY Out	-- 后台进程请求输出 */

struct sigaction {
	void (*sa_handler)(int);	/* 对应某信号指定要采取的行动 */
	sigset_t sa_mask;			/* 对信号的屏蔽码，在信号程序执行时将阻塞对这些信号的处理 */
	int sa_flags;				/* 改变信号处理过程的信号集 */
	void (*sa_restorer)(void);/* 恢复函数指针，由函数库Libc提供，用于清理用户态堆栈 */
};

#endif