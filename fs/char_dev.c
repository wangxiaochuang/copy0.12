#include <errno.h>
#include <sys/types.h>

#include <linux/sched.h>
#include <linux/kernel.h>

extern int tty_read(unsigned minor, char * buf, int count, unsigned short flags);
extern int tty_write(unsigned minor, char * buf, int count);
extern int lp_write(unsigned minor, char * buf, int count);

typedef int (*crw_ptr)(int, unsigned, char *, int, off_t *, unsigned short);

static int rw_ttyx(int rw, unsigned minor, char * buf, int count, off_t * pos) {
	return ((rw == READ) ? tty_read(minor, buf, count, flags) : tty_write(minor, buf, count));
}

static int rw_tty(int rw,unsigned minor,char * buf,int count, off_t * pos) {
	if (current->tty < 0)
		return -EPERM;
	return rw_ttyx(rw, current->tty, buf, count, pos, flags);
}

static int rw_memory(int rw, unsigned minor, char * buf, int count, off_t * pos) {
    return 0;
}

#define NRDEVS ((sizeof (crw_table))/(sizeof (crw_ptr)))

static crw_ptr crw_table[]={
	NULL,		/* nodev */
	rw_memory,	/* /dev/mem etc */
	NULL,		/* /dev/fd */
	NULL,		/* /dev/hd */
	rw_ttyx,	/* /dev/ttyx */
	rw_tty,		/* /dev/tty */
	rw_lp,		/* /dev/lp */
	NULL
};		/* unnamed pipes */

int char_read(struct inode *inode, struct file *filp, char *buf, int count) {
	unsigned int major, minor;
	crw_ptr call_addr;

	major = MAJOR(inode->i_rdev);
	minor = MINOR(inode->i_rdev);
	if (major >= NRDEVS)
		return -ENODEV;
	if (!(call_addr = crw_table[major]))
		return -ENODEV;
	return call_addr(READ, minor, buf, count, &filp->f_pos, filp->f_flags);
}

int char_write(struct inode *inode, struct file *filp, char *buf, int count) {
	unsigned int major, minor;
	crw_ptr call_addr;

	major = MAJOR(inode->i_rdev);
	minor = MINOR(inode->i_rdev);
	if (major >= NRDEVS)
		return -ENODEV;
	if (!(call_addr = crw_table[major]))
		return -ENODEV;
	return call_addr(WRITE, minor, buf, count, &filp->f_pos, filp->f_flags);
}