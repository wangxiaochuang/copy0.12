#include <errno.h>
#include <linux/sched.h>
#include <linux/tty.h>

int sys_pause(void);
int sys_close(int fd);

static inline int send_sig(long sig, struct task_struct * p,int priv) {		
	if (!p)
		return -EINVAL;
	if (!priv && (current->euid != p->euid) && !suser())
		return -EPERM;
	if ((sig == SIGKILL) || (sig == SIGCONT)) {
		if (p->state == TASK_STOPPED)
			p->state = TASK_RUNNING;
		p->exit_code = 0;
		p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
				(1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
	}
	if ((int) p->sigaction[sig-1].sa_handler == 1)
		return 0;
	if ((sig >= SIGSTOP) && (sig <= SIGTTOU)) 
		p->signal &= ~(1<<(SIGCONT-1));
	p->signal |= (1 << (sig - 1));
	return 0;
}

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

int sys_kill(int pid,int sig) {
	struct task_struct **p = NR_TASKS + task;
	int err, retval = 0;

	if (!pid)
		return(kill_pg(current->pid,sig,0));
	if (pid == -1) {
		while (--p > &FIRST_TASK)
			if ((err = send_sig(sig,*p,0)))
				retval = err;
		return(retval);
	}
	if (pid < 0) 
		return(kill_pg(-pid,sig,0));
	/* Normal kill */
	return(kill_proc(pid,sig,0));
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
// #ifdef DEBUG_PROC_TREE
	// audit_ptree();
// #endif
	schedule();
	for(;;);
}

void sys_exit(int error_code) {
	do_exit((error_code & 0xff) << 8);
}