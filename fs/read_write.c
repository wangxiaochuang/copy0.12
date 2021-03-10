#include <sys/stat.h>
#include <errno.h>

#include <linux/kernel.h>
#include <linux/sched.h>

#include <unistd.h>

extern int rw_char(int rw,int dev, char * buf, int count, off_t * pos);
extern int block_read(int dev, off_t * pos, char * buf, int count);
extern int block_write(int dev, off_t * pos, char * buf, int count);
extern int file_read(struct m_inode * inode, struct file * filp, char * buf, int count);
extern int file_write(struct m_inode * inode, struct file * filp, char * buf, int count);

int sys_lseek(unsigned int fd, off_t offset, unsigned int origin) {
	struct file * file;
	int tmp, mem_dev;

	if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(file->f_inode)) {
		return -EBADF;
	}
    if (origin > 2)
        return -EINVAL;
	if (file->f_inode->i_pipe) { /* 管道不能操作读写指针 */
		return -ESPIPE;
	}
    if (file->f_op && file->f_op->lseek)
        return file->f_op->lseek(file->f_inode, file, offset, origin);

    mem_dev = S_ISCHR(file->f_inode->i_mode);

    switch (origin) {
        case 0:
            if (offset < 0 && !mem_dev) return -EINVAL;
            file->f_pos = offset;
            break;
        case 1:
            if (file->f_pos + offset < 0 && !mem_dev) return -EINVAL;
            file->f_pos += offset;
            break;
        case 2:
            if ((tmp = file->f_inode->i_size + offset) < 0 && !mem_dev) {
                return -EINVAL;
            } 
            file->f_pos = tmp;
    }
    if (mem_dev && file->f_pos < 0)
        return 0;
    return file->f_pos;
}
int sys_read(unsigned int fd, char *buf, unsigned int count) {
    struct file *file;
    struct inode *inode;

    if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(inode = file->file->f_inode)) {
        return -EBADF;
    }
    // 不可读
    if (!(file->f_mode & 1))
        return -EBADF;
    if (!count) {
        return 0;
    }
    verify_area(buf, count);
    if (file->f_op && file->f_op->read)
        return file->f_op->read(inode, file, buf, count);
    if (inode->i_pipe)
        return pipe_read(inode, file, buf, count);
    if (S_ISCHR(inode->i_mode))
        return char_read(inode, file, buf, count);
    if (S_ISBLK(inode->i_mode))
        return block_read(inode, file, buf, count);
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
        return minix_file_read(inode, file, buf, count);
    }
    printk("(Read)inode->i_mode=%06o\n\r",inode->i_mode);
	return -EINVAL;
}
int sys_write(unsigned int fd, char * buf, unsigned int count) {
    struct file *file;
    struct inode *inode;

    if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(inode = file->f_inode))
        return -EBADF;
    
    if (!(file->f_mode & 2))
        return -EBADF;

    if (!count) {
        return 0;
    }

    if (file->f_op && file->f_op->write)
        return file->f_op->write(inode, file, buf, count);

    if (inode->i_pipe) {
        return pipe_write(inode, file, buf, count);
    }
    if (S_ISCHR(inode->i_mode)) {
        return char_write(inode, file, buf, count);
    }
    if (S_ISBLK(inode->i_mode)) {
        return block_write(inode, file, buf, count);
    }
    if (S_ISREG(inode->i_mode)) {
        return minix_file_write(inode, file, buf, count);
    }
    printk("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
	return -EINVAL;
}