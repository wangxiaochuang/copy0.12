#include <linux/types.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/segment.h>

asmlinkage int sys_write(unsigned int fd,char * buf,unsigned int count) {
    int error;
	struct file * file;
	struct inode * inode;
	
	if (fd>=NR_OPEN || !(file=current->filp[fd]) || !(inode=file->f_inode))
		return -EBADF;
	if (!(file->f_mode & 2))
		return -EBADF;
	if (!file->f_op || !file->f_op->write)
		return -EINVAL;
	if (!count)
		return 0;
    error = verify_area(VERIFY_READ, buf, count);
    if (error)
        return error;
    return file->f_op->write(inode, file, buf, count);
}