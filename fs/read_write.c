#include <sys/stat.h>
#include <errno.h>

#include <linux/kernel.h>
#include <linux/sched.h>

#include <unistd.h>

extern int rw_char(int rw,int dev, char * buf, int count, off_t * pos);
extern int file_read(struct m_inode * inode, struct file * filp, char * buf, int count);

int sys_lseek(unsigned int fd, off_t offset, int origin) {
	struct file * file;
	int tmp;

	if (fd >= NR_OPEN || !(file = current->filp[fd]) || !(file->f_inode)
	   || !IS_SEEKABLE(MAJOR(file->f_inode->i_dev))) {
		return -EBADF;
	}
	if (file->f_inode->i_pipe) { /* 管道不能操作读写指针 */
		return -ESPIPE;
	}

    switch (origin) {
        case SEEK_SET:
            if (offset < 0) {
                return -EINVAL;
            }
            file->f_pos = offset;
            break;
        case SEEK_CUR:
            if (file->f_pos + offset < 0) {
                return -EINVAL;
            }
            file->f_pos += offset;
            break;
        case SEEK_END:
            if ((tmp = file->f_inode->i_size + offset) < 0) {
                return -EINVAL;
            } 
            file->f_pos = tmp;
            break;
        default:
            return -EINVAL;
    }
    return file->f_pos;
}
int sys_read(unsigned int fd, char *buf, int count) {
    struct file *file;
    struct m_inode *inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd])) {
        return -EINVAL;
    }
    if (!count) {
        return 0;
    }
    verify_area(buf, count);
    inode = file->f_inode;
    if (inode->i_pipe) {
        return (file->f_mode & 1) ? read_pipe(inode, buf, count) : -EIO;
    }
    if (S_ISCHR(inode->i_mode)) {
        return rw_char(READ, inode->i_zone[0], buf, count, &file->f_pos);
    }
    if (S_ISBLK(inode->i_mode)) {
        return block_read(inode->i_zone[0], &file->f_pos, buf, count);
    }
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
        if (count + file->f_pos > inode->i_size) {
            count = inode->i_size - file->f_pos;
        }
        if (count <= 0) {
            return 0;
        }
        return file_read(inode, file, buf, count);
    }
    printk("no type match..");
    return -EINVAL;
}
int sys_write(unsigned int fd, char * buf, int count) {
    struct file *file;
    struct m_inode *inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd]))
        return -EINVAL;

    if (!count) {
        return 0;
    }

    inode = file->f_inode;
    if (S_ISCHR(inode->i_mode)) {
        return rw_char(WRITE, inode->i_zone[0], buf, count, &file->f_pos);
    }
    console_print("it is invalid type");
    return -EINVAL;
}