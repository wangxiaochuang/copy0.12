#include <asm/segment.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/minix_fs.h>
#include <linux/stat.h>

static int minix_dir_read(struct inode * inode, struct file * filp, char * buf, int count) {
	return -EISDIR;
}

static int minix_readdir(struct inode *, struct file *, struct dirent *, int);

static struct file_operations minix_dir_operations = {
	NULL,			/* lseek - default */
	minix_dir_read,		/* read */
	NULL,			/* write - bad */
	minix_readdir,		/* readdir */
	NULL,			/* select - default */
	NULL,			/* ioctl - default */
	NULL,			/* mmap */
	NULL,			/* no special open code */
	NULL,			/* no special release code */
	file_fsync		/* default fsync */
};

struct inode_operations minix_dir_inode_operations = {
	&minix_dir_operations,	/* default directory file-ops */
	minix_create,		/* create */
	minix_lookup,		/* lookup */
	minix_link,		/* link */
	minix_unlink,		/* unlink */
	minix_symlink,		/* symlink */
	minix_mkdir,		/* mkdir */
	minix_rmdir,		/* rmdir */
	minix_mknod,		/* mknod */
	minix_rename,		/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	NULL,			/* bmap */
	minix_truncate,		/* truncate */
	NULL			/* permission */
};

static int minix_readdir(struct inode *inode, struct file * filp,
	struct dirent * dirent, int count) {
    return 0;
}