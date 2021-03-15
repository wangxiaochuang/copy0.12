#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/interrupt.h>

#include <asm/segment.h>
#include <asm/system.h>

#include "kbd_kern.h"

#define MAX_TTYS 256

struct tty_struct *tty_table[MAX_TTYS];
struct termios *tty_termios[MAX_TTYS];
struct tty_ldisc ldiscs[NR_LDISCS];
// 256个tty的位图
int tty_check_write[MAX_TTYS/32];

int fg_console = 0;

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
    return 0;
}

static int tty_open(struct inode * inode, struct file * filp) {
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

void tty_bh_routine(void * unused) {
    return;
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