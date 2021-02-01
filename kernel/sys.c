#include <errno.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <string.h>

struct timezone sys_tz = { 0, 0};

extern int session_of_pgrp(int pgrp);

int sys_ftime() {
	return -ENOSYS;
}

int sys_break() {
	return -ENOSYS;
}

int sys_ptrace() {
	return -ENOSYS;
}

int sys_stty() {
	return -ENOSYS;
}

int sys_gtty() {
	return -ENOSYS;
}

int sys_rename() {
	return -ENOSYS;
}

int sys_prof() {
	return -ENOSYS;
}

int sys_setregid(int rgid, int egid) {
	if (rgid > 0) {
		if ((current->gid == rgid) || 
		    suser())
			current->gid = rgid;
		else
			return(-EPERM);
	}
	if (egid>0) {
		if ((current->gid == egid) ||
		    (current->egid == egid) ||
		    suser()) {
			current->egid = egid;
			current->sgid = egid;
		} else
			return(-EPERM);
	}
	return 0;
}

int sys_setgid(int gid) {
    if (suser())
        current->gid = current->egid = current->sgid = gid;
    else if ((gid == current->gid) || (gid == current->sgid))
        current->egid = gid;
    else
        return -EPERM;
    return 0;
}

int sys_acct() {
	return -ENOSYS;
}

int sys_phys() {
	return -ENOSYS;
}

int sys_lock() {
	return -ENOSYS;
}

int sys_mpx() {
	return -ENOSYS;
}

int sys_ulimit() {
	return -ENOSYS;
}

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

/**
 * 设置系统开机时间。提供的参数值减去系统已经运行的时间秒值就是开机时间秒值
 **/
int sys_stime(long * tptr) {
	if (!suser())
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies / HZ;
	jiffies_offset = 0;
	return 0;
}

/**
 * 获取当前任务运行时间统计值
 **/
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

/**
 * 为进程pid设置进程组号，只能设置当前进程或子进程
 **/
int sys_setpgid(int pid, int pgid) {
    int i;
    if (!pid)
        pid = current->pid;
    if (!pgid)
        pgid = current->pid;
    if (pgid < 0)
        return -EINVAL;
    for (i = 0; i < NR_TASKS; i++) {
        // 找到指定进程，且该进程是当前进程的子进程或就是当前进程
        if (task[i] && (task[i]->pid == pid) && 
            ((task[i]->p_pptr == current) || 
            (task[i] == current))) {
            // 不能是会话首领
            if (task[i]->leader)
                return -EPERM;
            // 指定进程与当前进程不是一个会话
            // 从一个进程组移到另外一个进程组，指定组id的会话不是当前进程的会话
            if ((task[i]->session != current->session) || 
                ((pgid != pid) && 
                (session_of_pgrp(pgid) != current->session)))
                return -EPERM;
            task[i]->pgrp = pgid;
            return 0;
        }
    }
    return -ESRCH;
}

int sys_getpgrp(void) {
	return current->pgrp;
}

int sys_setsid(void) {
    if (current->leader && !suser())
        return -EPERM;
    current->leader = 1;
    current->session = current->pgrp = current->pid;
    current->tty = -1;
    return current->pgrp;
}

int sys_getgroups(int gidsetsize, gid_t *grouplist) {
    int i;
    if (gidsetsize) {
        verify_area(grouplist, sizeof(gid_t) * gidsetsize);
    }
    for (i = 0; (i < NGROUPS) && (current->groups[i] != NOGROUP); i++, grouplist++) {
        if (gidsetsize) {
            if (i >= gidsetsize) return -EINVAL;
            put_fs_word(current->groups[i], (short *) grouplist);
        }
    }
    return i;
}

int sys_setgroups(int gidsetsize, gid_t *grouplist) {
    int i;
    if (!suser()) return -EPERM;
    if (gidsetsize > NGROUPS) return -EINVAL;
    for (i = 0; i < gidsetsize; i++, grouplist++) {
        current->groups[i] = get_fs_word((unsigned short *) grouplist);
    }
    if (i < NGROUPS) {
        current->groups[i] = NOGROUP;
    }
    return 0;
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

int sys_getrlimit(int resource, struct rlimit *rlim) {
    if (resource >= RLIM_NLIMITS) return -EINVAL;
    verify_area(rlim, sizeof *rlim);
    put_fs_long(current->rlim[resource].rlim_cur, (unsigned long *) rlim);
    put_fs_long(current->rlim[resource].rlim_max, ((unsigned long *) rlim) + 1);
    return 0;
}

int sys_setrlimit(int resource, struct rlimit *rlim) {
    struct rlimit new, *old;
    if (resource >= RLIM_NLIMITS)
        return -EINVAL;
    old = current->rlim + resource;
    new.rlim_cur = get_fs_long((unsigned long *) rlim);
    new.rlim_max = get_fs_long(((unsigned long *) rlim) + 1);
    // 新资源限额不能大于资源的最大额
    // 新资源的最大额不能大于资源的最大额
    // 超级用户随便设置
    if (((new.rlim_cur > old->rlim_max) || 
        (new.rlim_max > old->rlim_max)) && 
        !suser())
        return -EPERM;
    *old = new;
    return 0;
}

int sys_getrusage(int who, struct rusage *ru) {
	struct rusage r;
	unsigned long *lp, *lpend, *dest;

	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;
	verify_area(ru, sizeof *ru);
	memset((char *) &r, 0, sizeof(r));
	if (who == RUSAGE_SELF) {
		r.ru_utime.tv_sec = CT_TO_SECS(current->utime);
		r.ru_utime.tv_usec = CT_TO_USECS(current->utime);
		r.ru_stime.tv_sec = CT_TO_SECS(current->stime);
		r.ru_stime.tv_usec = CT_TO_USECS(current->stime);
	} else {
		r.ru_utime.tv_sec = CT_TO_SECS(current->cutime);
		r.ru_utime.tv_usec = CT_TO_USECS(current->cutime);
		r.ru_stime.tv_sec = CT_TO_SECS(current->cstime);
		r.ru_stime.tv_usec = CT_TO_USECS(current->cstime);
	}
    lp = (unsigned long *) &r;
    // 做个界限值使用
    lpend = (unsigned long *) (&r + 1);
    dest = (unsigned long *) ru;
    for (; lp < lpend; lp++, dest++)
        put_fs_long(*lp, dest);
    return 0;
}

/**
 * 取得系统当前时间
 *  tv: 含有秒和微秒两个字段
 *  tz: 带时区的时间
 **/
int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
	if (tv) {
		verify_area(tv, sizeof *tv);
		put_fs_long(startup_time + CT_TO_SECS(jiffies+jiffies_offset),
			    (unsigned long *) tv);
		put_fs_long(CT_TO_USECS(jiffies+jiffies_offset), 
			    ((unsigned long *) tv)+1);
	}
	if (tz) {
		verify_area(tz, sizeof *tz);
		put_fs_long(sys_tz.tz_minuteswest, (unsigned long *) tz);
		put_fs_long(sys_tz.tz_dsttime, ((unsigned long *) tz)+1);
	}
	return 0;
}


int sys_settimeofday(struct timeval *tv, struct timezone *tz) {
    static int firsttime = 1;    
    void adjust_clock();
    if (!suser()) return -EPERM;
    if (tz) {
        sys_tz.tz_minuteswest = get_fs_long((unsigned long *) tz);
        sys_tz.tz_dsttime = get_fs_long(((unsigned long *) tz) + 1);
        if (firsttime) {
            firsttime = 0;
            if (!tv)
                adjust_clock();
        }
    }
    if (tv) {
        int sec, usec;
        sec = get_fs_long((unsigned long *) tv);
        usec = get_fs_long(((unsigned long *) tv) + 1);
        startup_time = sec - jiffies / HZ;
        // 系统嘀嗒误差值 jiffies_offset
        jiffies_offset = usec * HZ / 1000000 - jiffies % HZ;
    }
    return 0;
}

void adjust_clock() {
	startup_time += sys_tz.tz_minuteswest * 60;
}

int sys_umask(int mask) {
    int old = current->umask;

    current->umask = mask & 0777;
    return (old);
}
