#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>
#include <errno.h>

int sys_sgetmask() {
	return current->blocked;
}

int sys_ssetmask(int newmask) {
	int old = current->blocked;
	current->blocked = newmask & ~(1<<(SIGKILL-1)) & ~(1<<(SIGSTOP-1));
	return old;
}

// 检测并取得进程收到的但被屏蔽的信号，还没处理的信号位图放入set中
int sys_sigpending(sigset_t *set) {
	verify_area(set, 4);
	put_fs_long(current->blocked & current->signal, (unsigned long *) set);
	return 0;
}

int sys_sigsuspend(int restart, unsigned long old_mask, unsigned long set) {
	extern int sys_pause(void);
	if (restart) {
		current->blocked = old_mask;
		return -EINTR;
	}
	*(&restart) = 0;
	*(&old_mask) = current->blocked;
	current->blocked = set;
	(void) sys_pause();
	return -ERESTARTNOINTR;
}

static inline void save_old(char * from, char * to) {
	int i;

	verify_area(to, sizeof(struct sigaction));
	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		put_fs_byte(*from,to);
		from++;
		to++;
	}
}

static inline void get_new(char * from, char * to) {
	int i;

	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		*(to++) = get_fs_byte(from++);
	}
}

int sys_signal(int signum, long handler, long restorer) {
	struct sigaction tmp;
	if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
		return -EINVAL;
	tmp.sa_handler = (void (*)(int)) handler;
	tmp.sa_mask = 0;
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
	tmp.sa_restorer = (void (*)(void)) restorer;
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
	return handler;
}

int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction) {
    struct sigaction tmp;
	if (signum<1 || signum>32 || signum==SIGKILL || signum==SIGSTOP)
		return -EINVAL;
	tmp = current->sigaction[signum-1];
	get_new((char *) action, (char *) (signum-1+current->sigaction));
	if (oldaction)
		save_old((char *) &tmp, (char *) oldaction);
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
	return 0;
}

int core_dump(long signr) {
	return(0);	/* We didn't do a dump */
}

/**
 * eax: 系统调用返回值, orig_eax: 系统调用号
 * ebx-ds: 中断开始执行压入的
 * eip-ss: 通过中断门cpu自动压入
 **/
int do_signal(long signr,long eax,long ebx, long ecx, long edx, long orig_eax,
	long fs, long es, long ds,
	long eip, long cs, long eflags,
	unsigned long * esp, long ss) {

    unsigned long sa_handler;
	long old_eip = eip;
	struct sigaction *sa = current->sigaction + signr - 1;
	int longs;

	unsigned long *tmp_esp;
	// 如果不是系统调用而是其他中断执行过程调用本函数，orig_eax就是-1，比如时间中断
	if ((orig_eax != -1) && 
		// 进程可以中断，但在继续执行后重新启动系统调用
		// 处理完信号后要求返回到原系统调用中继续执行，系统调用不会中断
		((eax == -ERESTARTSYS) || (eax == -ERESTARTNOINTR))) {				
        // 如果系统调用返回码是重启系统调用，但是sigaction含有标志不重启或信号值是关闭进程等
		if ((eax == -ERESTARTSYS) && ((sa->sa_flags & SA_INTERRUPT) || 
			signr < SIGCONT || signr > SIGTTOU))
			*(&eax) = -EINTR;
		else {
			// 恢复系统调用号，原程序指令指针回调即重新执行中断指令
			*(&eax) = orig_eax;
			*(&eip) = old_eip -= 2;
		}
	}
	sa_handler = (unsigned long) sa->sa_handler;
	// 如果处理方式是忽略
	if (sa_handler == 1)
		return(1);
	// 如果处理方式是默认
	if (!sa_handler) {
		switch (signr) {
		case SIGCONT:
		case SIGCHLD:
			return(1);  /* Ignore, ... */

		case SIGSTOP:
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
			current->state = TASK_STOPPED;
			current->exit_code = signr;
			// 如果父进程对SIGCHLD信号的处理方式不是SA_NOCLDSTOP（当子进程停止
			// 执行或又继续执行不要产生SIGCHLD信号）
			if (!(current->p_pptr->sigaction[SIGCHLD-1].sa_flags & 
					SA_NOCLDSTOP))
				current->p_pptr->signal |= (1<<(SIGCHLD-1));
			return(1);  /* Reschedule another event */

		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGIOT:
		case SIGFPE:
		case SIGSEGV:
			if (core_dump(signr))
				do_exit(signr|0x80);
			/* fall through */
		default:
			do_exit(signr);
		}
	}
	// 默认方式处理完了，下面是用户指定处理函数的方式
	// 如果是只执行一次
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_handler = NULL;
	*(&eip) = sa_handler;
	longs = (sa->sa_flags & SA_NOMASK) ? 7 : 8;
	*(&esp) -= longs;
	verify_area(esp, longs * 4);
	tmp_esp = esp;
	put_fs_long((long) sa->sa_restorer, tmp_esp++);
	put_fs_long(signr, tmp_esp++);
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked, tmp_esp++);
	put_fs_long(eax, tmp_esp++);
	put_fs_long(ecx, tmp_esp++);
	put_fs_long(edx, tmp_esp++);
	put_fs_long(eflags, tmp_esp++);
	put_fs_long(old_eip, tmp_esp++);
	current->blocked |= sa->sa_mask;
	return(0);
}