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
    int error;
	struct buffer_head * bh;

	*res_inode = NULL;
	if (!dir) {
		dir = current->root;
		dir->i_count++;
	}
	if (!inode) {
		iput(dir);
		return -ENOENT;
	}
	if (!S_ISLNK(inode->i_mode)) {
		iput(dir);
		*res_inode = inode;
		return 0;
	}
	if (current->link_count > 5) {
		iput(inode);
		iput(dir);
		return -ELOOP;
	}
	if (!(bh = minix_bread(inode, 0, 0))) {
		iput(inode);
		iput(dir);
		return -EIO;
	}
	iput(inode);
	current->link_count++;
	error = open_namei(bh->b_data, flag, mode, res_inode, dir);
	current->link_count--;
	brelse(bh);
	return error;
}

static int minix_readlink(struct inode * inode, char * buffer, int buflen) {
    return 0;
}