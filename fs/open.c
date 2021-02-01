#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>

#include <asm/segment.h>

int sys_ustat(int dev, struct ustat * ubuf) {
	return -ENOSYS;
}

/**
 * 修改文件访问和修改时间
 **/
int sys_utime(char *filename, struct utimbuf *times) {
	struct m_inode *inode;
	long actime, modtime;

    if (!(inode = namei(filename))) return -ENOENT;
    if (times) {
        actime = get_fs_long((unsigned long *) &times->actime);
        modtime = get_fs_long((unsigned long *) &times->modtime);
    } else {
        actime = modtime = CURRENT_TIME;
    }
    inode->i_atime = actime;
    inode->i_mtime = modtime;
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

/**
 * 检查文件的访问权限
 **/
int sys_access(const char * filename, int mode) {
    struct m_inode * inode;
	int res, i_mode;

    mode &= 0007;
    if (!(inode = namei(filename))) return -EACCES;
    i_mode = res = inode->i_mode & 0777;
    iput(inode);
    if (current->uid == inode->i_uid) {
        res >>= 6;
    } else if (current->gid == inode->i_gid) {
        // 原本写的6，是bug
        res >>= 3;
    }
    if ((res & 0007 & mode) == mode) return 0;
    // 是超级用户且屏蔽码执行位是0或者文件可以被任何人执行、搜索
    if ((!current->uid) && 
        (!(mode & 1) || (i_mode & 0111))) return 0;
    return -EACCES;
}

static int check_char_dev(struct m_inode * inode, int dev, int flag) {
    struct tty_struct *tty;
	int min;

    /* ttys are somewhat special (ttyxx major==4, tty major==5) */
	if (MAJOR(dev) == 4 || MAJOR(dev) == 5) {
        if (MAJOR(dev) == 5) {
			min = current->tty;
		} else {
			min = MINOR(dev);
		}
		if (min < 0) {
			return -1;
		}
        if ((IS_A_PTY_MASTER(min)) && inode->i_count > 1) {
            return -1;
        }
        tty = TTY_TABLE(min);
        if (!(flag & O_NOCTTY) &&
		    current->leader &&
		    current->tty<0 &&
		    tty->session==0) {
			current->tty = min;
			tty->session= current->session;
			tty->pgrp = current->pgrp;
		}
        if (flag & O_NONBLOCK) {
            TTY_TABLE(min)->termios.c_cc[VMIN] = 0;
            TTY_TABLE(min)->termios.c_cc[VTIME] = 0;
            TTY_TABLE(min)->termios.c_lflag &= ~ICANON;
        }
    }
    return 0;
}

int sys_chdir(const char * filename) {
    struct m_inode *inode;

    if (!(inode = namei(filename)))
        return -ENOENT;
    if (!S_ISDIR(inode->i_mode)) {
        iput(inode);
        return -ENOTDIR;
    }
    iput(current->pwd);
    current->pwd = inode;
    return 0;
}

int sys_chroot(const char * filename) {

}

int sys_chmod(const char * filename, int mode) {
    struct m_inode *inode;
    if (!(inode = namei(filename))) return -ENOENT;
    if ((current->euid != inode->i_uid) && !suser()) {
        iput(inode);
        return -EACCES;
    }
    inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

int sys_chown(const char * filename, int uid, int gid) {
    struct m_inode *inode;
    if (!(inode = namei(filename))) return -ENOENT;
    if (!suser()) {
        iput(inode);
        return -EACCES;
    }
    inode->i_uid = uid;
    inode->i_gid = gid;
    inode->i_dirt = 1;
    iput(inode);
    return 0;
}

int sys_open(const char * filename, int flag, int mode) {
    struct m_inode *inode;
    struct file *f;
    int i, fd;

    mode &= 0777 & ~current->umask;

    for (fd = 0; fd < NR_OPEN; fd++) {
        if (!current->filp[fd]) {
            break;
        }
    }
    if (fd >= NR_OPEN) {
        return -EINVAL;
    }
    current->close_on_exec &= ~(1<<fd);

    f = 0 + file_table;
    for (i = 0; i < NR_FILE; i++, f++) {
        if (!f->f_count) {
            break;
        }
    }
    if (i >= NR_FILE) {
        return -EINVAL;
    }
    (current->filp[fd] = f)->f_count++;
    if ((i = open_namei(filename, flag, mode, &inode)) < 0) {
        current->filp[fd] = NULL;
        f->f_count = 0;
        return i;
    }
    if (S_ISCHR(inode->i_mode)) {
        if (check_char_dev(inode, inode->i_zone[0], flag)) {
            iput(inode);
            current->filp[fd] = NULL;
			f->f_count = 0;
			return -EAGAIN;
        }
    }
    if (S_ISBLK(inode->i_mode)) {
        check_disk_change(inode->i_zone[0]);
	}
    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;
    return (fd);
}

int sys_creat(const char * pathname, int mode) {
	return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}

int sys_close(unsigned int fd) {
    struct file *filp;

    if (fd >= NR_OPEN) {
        return -EINVAL;
    }
    current->close_on_exec &= ~(1 << fd);
    if (!(filp = current->filp[fd])) {
        return -EINVAL;
    }
    current->filp[fd] = NULL;
    if (filp->f_count == 0) {
        panic("Close: file count is 0");
    }
    if (--filp->f_count) {
        return 0;
    }
    iput(filp->f_inode);
    return 0;
}