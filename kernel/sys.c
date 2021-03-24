#include <linux/sched.h>

asmlinkage unsigned long save_v86_state(struct vm86_regs * regs) {
    return 0;
}

int in_group_p(gid_t grp) {
    int i;

    if (grp == current->egid)
        return 1;
    for (i = 0; i < NGROUPS; i++) {
        if (current->groups[i] == NOGROUP)
            break;
        if (current->groups[i] == grp)
            return 1;
    }
    return 0;
}