#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>

int sys_ustat(int dev, struct ustat * ubuf) {
	return -ENOSYS;
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
        // @todo
    }
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
        // @todo
	}
    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;
    return (fd);
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