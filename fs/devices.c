#include <linux/fs.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/errno.h>

struct device_struct {
	const char * name;
	struct file_operations * fops;
};

static struct device_struct chrdevs[MAX_CHRDEV] = {
	{ NULL, NULL },
};

static struct device_struct blkdevs[MAX_BLKDEV] = {
	{ NULL, NULL },
};

int register_chrdev(unsigned int major, const char * name, struct file_operations *fops) {
    if (major >= MAX_CHRDEV)
        return -EINVAL;
    if (chrdevs[major].fops)
        return -EBUSY;
    chrdevs[major].name = name;
    chrdevs[major].fops = fops;
    return 0;
}

int register_blkdev(unsigned int major, const char * name, struct file_operations *fops) {
    if (major >= MAX_BLKDEV)
        return -EINVAL;
    if (blkdevs[major].fops)
        return -EBUSY;
        blkdevs[major].name = name;
        blkdevs[major].fops = fops;
        return 0;
}

int blkdev_open(struct inode * inode, struct file * filp) {
	return 0;
}

struct file_operations def_blk_fops = {
	NULL,		/* lseek */
	NULL,		/* read */
	NULL,		/* write */
	NULL,		/* readdir */
	NULL,		/* select */
	NULL,		/* ioctl */
	NULL,		/* mmap */
	blkdev_open,	/* open */
	NULL,		/* release */
};

struct inode_operations blkdev_inode_operations = {
	&def_blk_fops,		/* default file operations */
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
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};

int chrdev_open(struct inode * inode, struct file * filp) {
    return 0;
}

struct file_operations def_chr_fops = {
	NULL,		/* lseek */
	NULL,		/* read */
	NULL,		/* write */
	NULL,		/* readdir */
	NULL,		/* select */
	NULL,		/* ioctl */
	NULL,		/* mmap */
	chrdev_open,	/* open */
	NULL,		/* release */
};

struct inode_operations chrdev_inode_operations = {
	&def_chr_fops,		/* default file operations */
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
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};