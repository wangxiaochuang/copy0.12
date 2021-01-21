#include <linux/sched.h>

int in_group_p(gid_t grp) {
    int i;

    if (grp == current->egid) {
        return 1;
    }

    for (i = 0; i < NGROUPS; i++) {
        if (current->groups[i] == NOGROUP) {
            break;
        }
        if (current->groups[i] == grp) {
            return 1;
        }
    }
    return 0;
}