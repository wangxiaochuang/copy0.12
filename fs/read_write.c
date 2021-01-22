#include <sys/stat.h>
#include <errno.h>

#include <linux/kernel.h>
#include <linux/sched.h>

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