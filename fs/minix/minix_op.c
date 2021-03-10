#include <linux/fs.h>
#include <linux/minix_fs.h>

void minix_put_inode(struct inode *inode) {
	minix_truncate(inode);
	minix_free_inode(inode);
}

struct inode_operations minix_inode_operations = {
	minix_create,
	minix_lookup,
	minix_link,
	minix_unlink,
	minix_symlink,
	minix_mkdir,
	minix_rmdir,
	minix_mknod,
	minix_rename,
	minix_readlink,
	minix_open,
	minix_release,
	minix_follow_link,
	minix_bmap,
	minix_truncate,
	minix_write_inode,
	minix_put_inode
};

struct file_operations minix_file_operations = {
	NULL,	/* lseek */
	NULL,	/* read */
	NULL,	/* write */
	minix_readdir
};