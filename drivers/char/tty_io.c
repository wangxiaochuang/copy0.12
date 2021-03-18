#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/interrupt.h>

#include <asm/segment.h>
#include <asm/system.h>
#include <asm/bitops.h>

#include "kbd_kern.h"

#define CONSOLE_DEV MKDEV(TTY_MAJOR,0)
#define MAX_TTYS 256

struct tty_struct *tty_table[MAX_TTYS];
struct termios *tty_termios[MAX_TTYS];
struct tty_ldisc ldiscs[NR_LDISCS];
// 256个tty的位图
int tty_check_write[MAX_TTYS/32];

int fg_console = 0;
struct tty_struct * redirect = NULL;
struct wait_queue * keypress_wait = NULL;

static int tty_read(struct inode *, struct file *, char *, int);
static int tty_write(struct inode *, struct file *, char *, int);
static int tty_select(struct inode *, struct file *, int, select_table *);
static int tty_open(struct inode *, struct file *);
static void tty_release(struct inode *, struct file *);

int tty_register_ldisc(int disc, struct tty_ldisc *new_ldisc) {
    if (disc < N_TTY || disc >= NR_LDISCS)
        return -EINVAL;
    if (new_ldisc) {
        ldiscs[disc] = *new_ldisc;
        ldiscs[disc].flags |= LDISC_FLAG_DEFINED;
    } else {
        memset(&ldiscs[disc], 0, sizeof(struct  tty_ldisc));
    }
    return 0;
}

static int tty_lseek(struct inode * inode, struct file * file, off_t offset, int orig) {
	return -ESPIPE;
}

int tty_ioctl(struct inode * inode, struct file * file,
	unsigned int cmd, unsigned long arg) {
    return 0;
}
static struct file_operations tty_fops = {
	tty_lseek,
	tty_read,
	tty_write,
	NULL,		/* tty_readdir */
	tty_select,
	tty_ioctl,
	NULL,		/* tty_mmap */
	tty_open,
	tty_release
};

static void copy_to_cooked(struct tty_struct * tty) {
    return;
}

static int read_chan(struct tty_struct *tty, struct file *file,
		     unsigned char *buf, unsigned int nr) {
    return 0;
}

static int write_chan(struct tty_struct * tty, struct file * file,
		      unsigned char * buf, unsigned int nr) {
    return 0;
}

static int tty_read(struct inode * inode, struct file * file, char * buf, int count) {
    return 0;
}

static int tty_write(struct inode * inode, struct file * file, char * buf, int count) {
    int dev, i, is_console;
    struct tty_struct *tty;

    dev = file->f_rdev;
    is_console = (inode->i_rdev == CONSOLE_DEV);
    if (MAJOR(dev) != TTY_MAJOR) {
        printk("tty_write: pseudo-major != TTY_MAJOR\n");
		return -EINVAL;
    }
    dev = MINOR(dev);
    if (is_console && redirect)
        tty = redirect;
    else
        tty = TTY_TABLE(dev);
    if (!tty || !tty->write || (tty->flags & (1 << TTY_IO_ERROR)))
        return -EIO;
    
    if (ldiscs[tty->disc].write)
        i = (ldiscs[tty->disc].write)(tty, file, (unsigned char *) buf, (unsigned int) count);
    else
        i = -EIO;
    if (i > 0)
        inode->i_mtime = CURRENT_TIME;
    return i;
}

static int init_dev(int dev) {
    return 0;
}

static void release_dev(int dev, struct file * filp) {

}

static int tty_open(struct inode * inode, struct file * filp) {
    struct tty_struct *tty;
    int major, minor;
    int noctty, retval;

retry_open:
    minor = MINOR(inode->i_rdev);
    major = MAJOR(inode->i_rdev);
    noctty = filp->f_flags & O_NOCTTY;
    if (major == TTYAUX_MAJOR) {
        if (!minor) {
            major = TTY_MAJOR;
            minor = current->tty;
        }
    } else if (major == TTY_MAJOR) {
        if (!minor) {
            minor = fg_console + 1;
            noctty = 1;
        }
    } else {
        printk("Bad major #%d in tty_open\n", MAJOR(inode->i_rdev));
		return -ENODEV;
    }
    if (minor <= 0)
        return -ENXIO;
    if (IS_A_PTY_MASTER(minor))
        noctty = 1;
    filp->f_rdev = (major << 8) | minor;
    retval = init_dev(minor);
    if (retval)
        return retval;
    tty = tty_table[minor];
    if (test_bit(TTY_EXCLUSIVE, &tty->flags) && !suser())
        return -EBUSY;
    if (tty->open) {
        retval = tty->open(tty, filp);
    } else {
        retval = -ENODEV;
    }
    if (retval) {
        release_dev(minor, filp);
        if (retval != -ERESTARTSYS)
            return retval;
        if (current->signal & ~current->blocked)
            return retval;
        schedule();
        goto retry_open;
    }
    if (!noctty &&
        current->leader &&
        current->tty < 0 &&
        tty->session == 0) {
            current->tty = minor;
            tty->session = current->session;
            tty->pgrp = current->pgrp;
        }
        filp->f_rdev = MKDEV(TTY_MAJOR, minor);
    return 0;
}

static int tty_select(struct inode * inode, struct file * filp, int sel_type, select_table * wait) {
    return 0;
}

static void tty_release(struct inode * inode, struct file * filp) {
    return;
}

static int normal_select(struct tty_struct * tty, struct inode * inode,
			 struct file * file, int sel_type, select_table *wait) {
    return 0;
}

int tty_write_data(struct tty_struct *tty, char *bufp, int buflen,
		    void (*callback)(void * data), void * callarg) {
    #define VLEFT ((tail-head-1)&(TTY_BUF_SIZE-1))
}

void tty_bh_routine(void * unused) {
    int i, j, line, mask;
    int head, tail, count;
    unsigned char *p;
    struct tty_struct *tty;
    for (i = 0, line = 0; i < MAX_TTYS / 32; i++) {
        if (!tty_check_write[i]) {
            line += 32;
            continue;
        }
        for (j = 0, mask = 0; j < 32; j++, line++, mask <<= 1) {
            if (clear_bit(j, &tty_check_write[i])) {
                tty = tty_table[line];
                if (!tty || !tty->write_data_cnt)
                    continue;
                cli();
                head = tty->write_q.head;
                tail = tty->write_q.tail;
                count = tty->write_data_cnt;
                p = tty->write_data_ptr;
                 while (count && VLEFT > 0) {
                     tty->write_q.buf[head++] = *p++;
                     head &= TTY_BUF_SIZE-1;
                     count--;
                 }
                 tty->write_q.head = head;
                 tty->write_data_ptr = p;
                 tty->write_data_cnt = count;
                 sti();
                 if (!count)
                    (tty->write_data_callback)(tty->write_data_arg);
            }
        }
    }

}

static struct tty_ldisc tty_ldisc_N_TTY = {
	0,			/* flags */
	NULL,			/* open */
	NULL,			/* close */
	read_chan,		/* read */
	write_chan,		/* write */
	NULL,			/* ioctl */
	normal_select,		/* select */
	copy_to_cooked		/* handler */
};

/**
 * kmem_start可以开始使用的内存，1M以后用作页表，紧接着就是这里了
 **/
long tty_init(long kmem_start) {
    int i;
    if (sizeof(struct tty_struct) > PAGE_SIZE)
		    panic("size of tty structure > PAGE_SIZE!");
    if (register_chrdev(TTY_MAJOR, "tty", &tty_fops))
        panic("unable to get major %d for tty device", TTY_MAJOR);
    if (register_chrdev(TTYAUX_MAJOR, "tty", &tty_fops))
        panic("unable to get major %d for tty device", TTYAUX_MAJOR);
    for (i = 0; i < MAX_TTYS; i++) {
        tty_table[i] = 0;
        tty_termios[i] = 0;
    }
    memset(tty_check_write, 0, sizeof(tty_check_write));
    bh_base[TTY_BH].routine = tty_bh_routine;
    memset(ldiscs, 0, sizeof(ldiscs));
    (void) tty_register_ldisc(N_TTY, &tty_ldisc_N_TTY);

    kmem_start = kbd_init(kmem_start);
    kmem_start = con_init(kmem_start);
    return kmem_start;
}