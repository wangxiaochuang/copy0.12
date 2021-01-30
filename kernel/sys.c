#include <errno.h>
#include <linux/sched.h>
#include <linux/config.h>
#include <asm/segment.h>
#include <sys/times.h>
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

int sys_ulimit() {
	return -ENOSYS;
}

int sys_time(long * tloc) {
    int i;

    i = CURRENT_TIME;
    if (tloc) {
        verify_area(tloc, 4);
        put_fs_long(i, (unsigned long *) tloc);
    }
    return i;
}

int sys_setreuid(int ruid, int euid) {
	int old_ruid = current->uid;
	
	if (ruid>0) {
		if ((current->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			current->uid = ruid;
		else
			return(-EPERM);
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (current->euid == euid) ||
		    suser()) {
			current->euid = euid;
			current->suid = euid;
		} else {
			current->uid = old_ruid;
			return(-EPERM);
		}
	}
	return 0;
}

int sys_setuid(int uid) {
	if (suser())
		current->uid = current->euid = current->suid = uid;
	else if ((uid == current->uid) || (uid == current->suid))
		current->euid = uid;
	else
		return -EPERM;
	return(0);
}

int sys_stime(long * tptr) {
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
	jiffies_offset = 0;
	return 0;
}

int sys_times(struct tms * tbuf) {
	if (tbuf) {
		verify_area(tbuf,sizeof *tbuf);
		put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
		put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
		put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
		put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	}
	return jiffies;
}

int sys_brk(unsigned long end_data_seg) {
    if (end_data_seg >= current->end_code &&
        end_data_seg < current->start_stack - 16384)
        current->brk = end_data_seg;
    return current->brk;
}

int sys_setsid(void) {
    if (current->leader && !suser())
        return -EPERM;
    current->leader = 1;
    current->session = current->pgrp = current->pid;
    current->tty = -1;
    return current->pgrp;
}

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

int sys_umask(int mask) {
    int old = current->umask;

    current->umask = mask & 0777;
    return (old);
}