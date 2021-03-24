#include <asm/segment.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/minix_fs.h>
#include <linux/stat.h>

static int minix_readlink(struct inode *, char *, int);
static int minix_follow_link(struct inode *, struct inode *, int, int, struct inode **);

struct inode_operations minix_symlink_inode_operations = {
	NULL,			/* no file-operations */
	NULL,			/* create */
	NULL,			/* lookup */
	NULL,			/* link */
	NULL,			/* unlink */
	NULL,			/* symlink */
	NULL,			/* mkdir */
	NULL,			/* rmdir */
	NULL,			/* mknod */
	NULL,			/* rename */
	minix_readlink,		/* readlink */
	minix_follow_link,	/* follow_link */
	NULL,			/* bmap */
	NULL,			/* truncate */
	NULL			/* permission */
};

static int minix_follow_link(struct inode * dir, struct inode * inode,
	int flag, int mode, struct inode ** res_inode) {
        return 0;
    }

static int minix_readlink(struct inode * inode, char * buffer, int buflen) {
    return 0;
}