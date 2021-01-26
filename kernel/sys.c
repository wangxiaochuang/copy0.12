#include <errno.h>
#include <linux/sched.h>
#include <linux/config.h>
#include <asm/segment.h>
#include <sys/utsname.h>

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

static struct utsname thisname = {
	UTS_SYSNAME, UTS_NODENAME, UTS_RELEASE, UTS_VERSION, UTS_MACHINE
};

int sys_uname(struct utsname * name) {
    int i;
    if (!name) return -ERROR;
    verify_area(name, sizeof *name);
    for (i = 0; i < sizeof *name; i++) {
        put_fs_byte(((char *) &thisname)[i], i + (char *) name);
    }
    return 0;
}

int sys_sethostname(char *name, int len) {
    int i;
    if (!suser())
        return -EPERM;
    if (len > MAXHOSTNAMELEN)
        return -EINVAL;
    for (i = 0; i < len; i++) {
        if ((thisname.nodename[i] = get_fs_byte(name+i)) == 0)
            break;
    }
    if (thisname.nodename[i]) {
        thisname.nodename[i > MAXHOSTNAMELEN ? MAXHOSTNAMELEN : i] = 0;
    }
    return 0;
}