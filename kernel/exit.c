#define DEBUG_PROC_TREE	/* 定义符号"调度进程树" */

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);
int sys_close(int fd);

int unimpl(int nr) {
	panic("unimpl syscall %d", nr);
}

void release(struct task_struct *p) {
	int i;
	if (!p) return;
	if (p == current) {
		printk("task releasing itself\n\r");
		return;
	}
	for (i = 1; i < NR_TASKS; i++) {
		if (task[i] == p) {
			task[i] = NULL;
			// parent => young <=> p <=> old
			if (p->p_osptr)
				p->p_osptr->p_ysptr = p->p_ysptr;
			if (p->p_ysptr) {
				p->p_ysptr->p_osptr = p->p_osptr;
			} else {
				p->p_pptr->p_cptr = p->p_osptr;
			}
			free_page((long) p);
			schedule();
			return;
		}
	}
	panic("trying to release non-existent task");
}

#ifdef DEBUG_PROC_TREE

/**
 * 找到任务p返回0，找不到返回1
 **/
int bad_task_ptr(struct task_struct *p) {
	int i;
	if (!p) return 0;
	for (i = 0; i < NR_TASKS; i++)
		if (task[i] == p) return 0;
	return 1;
}

void audit_ptree() {
	for (int i = 1; i < NR_TASKS; i++) {
		if (!task[i])
			continue;
		// 找不到父进程
		if (bad_task_ptr(task[i]->p_pptr))
			printk("Warning, pid %d's parent link is bad\n",
				task[i]->pid);
		// 找不到子进程
		if (bad_task_ptr(task[i]->p_cptr))
			printk("Warning, pid %d's child link is bad\n",
				task[i]->pid);
		// 找不到年轻兄弟进程
		if (bad_task_ptr(task[i]->p_ysptr))
			printk("Warning, pid %d's ys link is bad\n",
				task[i]->pid);
		// 找不到年长兄弟进程
		if (bad_task_ptr(task[i]->p_osptr))
			printk("Warning, pid %d's os link is bad\n",
				task[i]->pid);
		// 父进程就是自己
		if (task[i]->p_pptr == task[i])
			printk("Warning, pid %d parent link points to self\n");
		// 子进程就是自己
		if (task[i]->p_cptr == task[i])
			printk("Warning, pid %d child link points to self\n");
		// 年轻兄弟进程就是自己
		if (task[i]->p_ysptr == task[i])
			printk("Warning, pid %d ys link points to self\n");
		// 年长兄弟进程就是自己
		if (task[i]->p_osptr == task[i])
			printk("Warning, pid %d os link points to self\n");
		if (task[i]->p_osptr) {
			// 年长兄弟进程的父进程不是自己的父进程
			if (task[i]->p_pptr != task[i]->p_osptr->p_pptr)
				printk(
			"Warning, pid %d older sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_osptr->p_ysptr != task[i])
				printk(
		"Warning, pid %d older sibling %d has mismatched ys link\n",
				task[i]->pid, task[i]->p_osptr->pid);
		}
		if (task[i]->p_ysptr) {
			// 年轻兄弟进程的父进程不是自己的父进程
			if (task[i]->p_pptr != task[i]->p_ysptr->p_pptr)
				printk(
			"Warning, pid %d younger sibling %d parent is %d\n",
				task[i]->pid, task[i]->p_osptr->pid,
				task[i]->p_osptr->p_pptr->pid);
			if (task[i]->p_ysptr->p_osptr != task[i])
				printk(
		"Warning, pid %d younger sibling %d has mismatched os link\n",
				task[i]->pid, task[i]->p_ysptr->pid);
		}
		if (task[i]->p_cptr) {
			// 子进程的父进程不是自己
			if (task[i]->p_cptr->p_pptr != task[i])
				printk(
			"Warning, pid %d youngest child %d has mismatched parent link\n",
				task[i]->pid, task[i]->p_cptr->pid);
			if (task[i]->p_cptr->p_ysptr)
				printk(
			"Warning, pid %d youngest child %d has non-NULL ys link\n",
				task[i]->pid, task[i]->p_cptr->pid);
		}
	}
}
#endif

/**
 * priv: 强制发送信号的标志（即不需要考虑进程用户属性或级别）
 **/
static inline int send_sig(long sig, struct task_struct * p,int priv) {		
	if (!p)
		return -EINVAL;
	if (!priv && (current->euid != p->euid) && !suser())
		return -EPERM;
	if ((sig == SIGKILL) || (sig == SIGCONT)) {
		if (p->state == TASK_STOPPED)
			p->state = TASK_RUNNING;
		p->exit_code = 0;
		// 复位会导致进程停止的信号
		p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
				(1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
	}
	// 发送的信号是被忽略的，就不用发送了
	if ((int) p->sigaction[sig-1].sa_handler == 1)
		return 0;
	// 如果是让进程停止的信号，说明是让接受信号的进程p停止运行，因此需要复位继续运行的信号
	if ((sig >= SIGSTOP) && (sig <= SIGTTOU)) 
		p->signal &= ~(1<<(SIGCONT-1));
	p->signal |= (1 << (sig - 1));
	return 0;
}

/**
 * 找到进程组pgrp的会话进程
 **/
int session_of_pgrp(int pgrp) {
	struct task_struct **p;
	for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
		if ((*p)->pgrp == pgrp)
			return ((*p)->session);
	}
	return -1;
}

// 给进程组号是pgrp的进程发送信号，如果遇到错误，值会返回最后一个错误
int kill_pg(int pgrp, int sig, int priv) {
	struct task_struct **p;
	int err,retval = -ESRCH;
	int found = 0;

	if (sig<1 || sig>32 || pgrp<=0)
		return -EINVAL;
	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if ((*p)->pgrp == pgrp) {
			if (sig && (err = send_sig(sig, *p, priv)))
				retval = err;
			else
				found++;
		}
	return (found ? 0 : retval);
}

int kill_proc(int pid, int sig, int priv) {
 	struct task_struct **p;

	if (sig<1 || sig>32)
		return -EINVAL;
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if ((*p)->pid == pid)
			return(sig ? send_sig(sig,*p,priv) : 0);
	return(-ESRCH);
}

int sys_kill(int pid, int sig) {
	struct task_struct **p = NR_TASKS + task;
	int err, retval = 0;

	// pid为0，表示当前进程是进程组组长，因此需要给组内所有进程强制发送信号sig
	if (!pid)
		return(kill_pg(current->pid, sig, 0));
	// pid为-1，表示信号要发送给除1号进程外的所有进程，如果发生错误，只会记录最后一个错误
	if (pid == -1) {
		while (--p > &FIRST_TASK)
			if ((err = send_sig(sig, *p, 0)))
				retval = err;
		return(retval);
	}
	// pid < -1，表示信号要发送给进程组号为-pid的所有进程
	if (pid < 0) 
		return(kill_pg(-pid, sig, 0));
	/* Normal kill */
	return(kill_proc(pid, sig, 0));
}

int is_orphaned_pgrp(int pgrp) {
	struct task_struct **p;

	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!(*p) ||
		    ((*p)->pgrp != pgrp) || 
		    ((*p)->state == TASK_ZOMBIE) ||
		    ((*p)->p_pptr->pid == 1))
			continue;
		if (((*p)->p_pptr->pgrp != pgrp) &&
		    ((*p)->p_pptr->session == (*p)->session))
			return 0;
	}
	return(1);
}

static int has_stopped_jobs(int pgrp) {
	struct task_struct ** p;

	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if ((*p)->pgrp != pgrp)
			continue;
		if ((*p)->state == TASK_STOPPED)
			return(1);
	}
	return(0);
}

__attribute__ ((noreturn))
void do_exit(long code) {
	struct task_struct *p;
	int i;

	free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
	for (i = 0; i < NR_OPEN; i++)
		if (current->filp[i]) 
			sys_close(i);
	iput(current->pwd);
	current->pwd = NULL;
	iput(current->root);
	current->root = NULL;
	iput(current->executable);
	current->executable = NULL;
	iput(current->library);
	current->library = NULL;
	current->state = TASK_ZOMBIE;
	current->exit_code = code;

	if ((current->p_pptr->pgrp != current->pgrp) && 
		(current->p_pptr->session == current->session) && 
		is_orphaned_pgrp(current->pgrp) && 
		has_stopped_jobs(current->pgrp)) {
		kill_pg(current->pgrp, SIGHUP, 1);
		kill_pg(current->pgrp, SIGCONT, 1);
	}

	current->p_pptr->signal |= (1 << (SIGCHLD - 1));

	// 当前进程有子进程
	if ((p = current->p_cptr)) {
		while (1) {
			// 1号进程成为子进程的父进程
			p->p_pptr = task[1];
			if (p->state == TASK_ZOMBIE)
				task[1]->signal |= (1 << (SIGCHLD - 1));
			if ((p->pgrp != current->pgrp) &&
			    (p->session == current->session) &&
			    is_orphaned_pgrp(p->pgrp) &&
			    has_stopped_jobs(p->pgrp)) {
				kill_pg(p->pgrp,SIGHUP,1);
				kill_pg(p->pgrp,SIGCONT,1);
			}
			// 子进程有兄弟进程，则循环处理
			if (p->p_osptr) {
				p = p->p_osptr;
				continue;
			}
			p->p_osptr = task[1]->p_cptr;
			task[1]->p_cptr->p_ysptr = p;
			task[1]->p_cptr = current->p_cptr;
			current->p_cptr = 0;
			break;
		}
	}
	if (current->leader) {
		struct task_struct **p;
		struct tty_struct *tty;

		if (current->tty >= 0) {
			tty = TTY_TABLE(current->tty);
			if (tty->pgrp > 0)
				kill_pg(tty->pgrp, SIGHUP, 1);
			tty->pgrp = 0;
			tty->session = 0;
		}
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if ((*p)->session == current->session)
				(*p)->tty = -1;
	}
	if (last_task_used_math == current)
		last_task_used_math = NULL;	
#ifdef DEBUG_PROC_TREE
	audit_ptree();
#endif
	schedule();
	for(;;);
}

void sys_exit(int error_code) {
	do_exit((error_code & 0xff) << 8);
}

/**
 * 等待子进程pid进程退出
 **/
int sys_waitpid(pid_t pid, unsigned long * stat_addr, int options) {
	int flag;
	struct task_struct *p;
	unsigned long oldblocked;

	verify_area(stat_addr, 4);
repeat:
	flag = 0;
	for (p = current->p_cptr; p; p = p->p_osptr) {
		// 等待指定进程pid
		if (pid > 0) {
			if (p->pid != pid)
				continue;
		// 等待当前进程所在进程组的进程
		} else if (!pid) {
			if (p->pgrp != current->pgrp)
				continue;
		// 等待指定进程组的进程
		} else if (pid != -1) {
			if (p->pgrp != -pid)
				continue;
		}
		// 等待任意子进程
		switch (p->state) {
			case TASK_STOPPED:
				if (!(options & WUNTRACED) ||
					!p->exit_code)
					continue;
				put_fs_long((p->exit_code << 8) | 0x7f,
					stat_addr);
				p->exit_code = 0;
				return p->pid;
			case TASK_ZOMBIE:
				current->cutime += p->utime;
				current->cstime += p->stime;
				flag = p->pid;
				put_fs_long(p->exit_code, stat_addr);
				release(p);
#ifdef DEBUG_PROC_TREE
				audit_ptree();
#endif
				return flag;
			default:
				flag = 1;
				continue;
		}
	}
	if (flag) {
		if (options & WNOHANG)
			return 0;
		current->state = TASK_INTERRUPTIBLE;
		oldblocked = current->blocked;
		current->blocked = ~(1 << (SIGCHLD - 1));
		schedule();
		current->blocked = oldblocked;
		if (current->signal & ~(current->blocked | (1 << (SIGCHLD - 1)))) {
	printk("receive signal SIGCHLD, restart %d\n", current->pid);
			return -ERESTARTSYS;
		} else {
	printk("no SIGCHLD, repeat %d\n", current->pid);
			goto repeat;
		}
	}
	return -ECHILD;
}