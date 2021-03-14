#include <linux/sched.h>
#include <linux/ptrace.h>

asmlinkage int do_signal(unsigned long oldmask, struct pt_regs * regs) {
    return 0;
}