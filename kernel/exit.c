#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>

static int generate(unsigned long sig, struct task_struct * p) {
	unsigned long mask = 1 << (sig-1);
	struct sigaction * sa = sig + p->sigaction - 1;

	/* always generate signals for traced processes ??? */
	if (p->flags & PF_PTRACED) {
		p->signal |= mask;
		return 1;
	}
	/* don't bother with ignored signals (but SIGCHLD is special) */
	if (sa->sa_handler == SIG_IGN && sig != SIGCHLD)
		return 0;
	/* some signals are ignored by default.. (but SIGCONT already did its deed) */
	if ((sa->sa_handler == SIG_DFL) &&
	    (sig == SIGCONT || sig == SIGCHLD || sig == SIGWINCH))
		return 0;
	p->signal |= mask;
	return 1;
}

int send_sig(unsigned long sig, struct task_struct * p, int priv) {
    if (!p || sig > 32)
        return -EINVAL;
    if (!priv && ((sig != SIGCONT) || (current->session != p->session)) &&
        (current->euid != p->euid) && (current->uid != p->uid) && !suser())
        return -EPERM;
    if (!sig)
        return 0;
    if ((sig == SIGKILL) || (sig == SIGCONT)) {
        if (p->state == TASK_STOPPED)
            p->state = TASK_RUNNING;
        p->exit_code = 0;
        p->signal &= ~( (1<<(SIGSTOP-1)) | (1<<(SIGTSTP-1)) |
				(1<<(SIGTTIN-1)) | (1<<(SIGTTOU-1)) );
    }
    /* Depends on order SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU */
	if ((sig >= SIGSTOP) && (sig <= SIGTTOU)) 
		p->signal &= ~(1<<(SIGCONT-1));
    /* Actually generate the signal */
    generate(sig, p);
    return 0;
}

NORET_TYPE void do_exit(long code) {
	mypanic("i am here, panic");
}