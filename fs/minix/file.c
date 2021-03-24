#include <asm/segment.h>
#include <asm/system.h>

#include <linux/sched.h>
#include <linux/minix_fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/locks.h>

#define	NBUF	32

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#include <linux/fs.h>
#include <linux/minix_fs.h>

static int minix_file_read(struct inode *, struct file *, char *, int);
static int minix_file_write(struct inode *, struct file *, char *, int);

static struct file_operations minix_file_operations = {
	NULL,			/* lseek - default */
	minix_file_read,	/* read */
	minix_file_write,	/* write */
	NULL,			/* readdir - bad */
	NULL,			/* select - default */
	NULL,			/* ioctl - default */
	generic_mmap,  		/* mmap */
	NULL,			/* no special open is needed */
	NULL,			/* release */
	minix_sync_file		/* fsync */
};

struct inode_operations minix_file_inode_operations = {
	&minix_file_operations,	/* default file operations */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	minix_bmap,		/* bmap */
	minix_truncate,		/* truncate */
	NULL			/* permission */
};

static int minix_file_read(struct inode * inode, struct file * filp, char * buf, int count) {
    return 0;
}

static int minix_file_write(struct inode * inode, struct file * filp, char * buf, int count) {
    return 0;
}