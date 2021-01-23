#include <errno.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <asm/segment.h>

static void cp_stat(struct m_inode * inode, struct stat * statbuf) {
    struct stat tmp;

    verify_area(statbuf, sizeof (struct stat));
    tmp.st_dev      = inode->i_dev;
    tmp.st_ino      = inode->i_num;
    tmp.st_mode     = inode->i_mode;
    tmp.st_nlink    = inode->i_nlinks;
    tmp.st_uid      = inode->i_uid;
    tmp.st_gid      = inode->i_gid;
    tmp.st_rdev     = inode->i_zone[0];
    tmp.st_size     = inode->i_size;
    tmp.st_atime	= inode->i_atime;
	tmp.st_mtime	= inode->i_mtime;
	tmp.st_ctime	= inode->i_ctime;

    for (int i = 0; i < sizeof(tmp); i++) {
        put_fs_byte(((char *) &tmp)[i], i + (char *) statbuf);
    }
}
int sys_stat(char * filename, struct stat *statbuf) {
    struct m_inode *inode;
    if (!(inode = namei(filename))) {
        return -ENOENT;
    }
    cp_stat(inode, statbuf);
    iput(inode);
    return 0;
}
int sys_lstat(char * filename, struct stat * statbuf) {
	struct m_inode * inode;

	if (!(inode = lnamei(filename))) {
		return -ENOENT;
	}
	cp_stat(inode, statbuf);
	iput(inode);
	return 0;
}
int sys_fstat(unsigned int fd, struct stat * statbuf) {
    struct file *f;
    struct m_inode *inode;

    if (fd >= NR_OPEN || !(f = current->filp[fd]) || !(inode = f->f_inode)) {
		return -EBADF;
	}
    cp_stat(inode, statbuf);
    return 0;
}