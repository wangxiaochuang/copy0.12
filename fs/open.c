#include <linux/vfs.h>
#include <linux/types.h>
#include <linux/utime.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/tty.h>
#include <linux/time.h>

#include <asm/segment.h>

int do_open(const char * filename, int flags, int mode) {
    struct inode *inode;
    struct file *f;
    int flag, error, fd;

    for (fd = 0; fd < NR_OPEN; fd++)
        if (!current->filp[fd])
            break;
    if (fd >= NR_OPEN)
        return -EMFILE;
    FD_CLR(fd, &current->close_on_exec);
    f = get_empty_filp(); 
    if (!f)
        return -ENFILE;
    current->filp[fd] = f;
    f->f_flags = flag = flags;
    f->f_mode = (flag + 1) & O_ACCMODE;
    if (f->f_mode)
        flag++;
    if (flag & (O_TRUNC | O_CREAT))
        flag |= 2;
    error = open_namei(filename, flag, mode, &inode, NULL);
    if (error) {
        current->filp[fd] = NULL;
        f->f_count--;
        return error;
    }
    f->f_inode = inode;
    f->f_pos = 0;
    f->f_reada = 0;
    f->f_op = NULL;
    if (inode->i_op)
        f->f_op = inode->i_op->default_file_ops;
    if (f->f_op && f->f_op->open) {
        error = f->f_op->open(inode, f);
        if (error) {
            iput(inode);
            f->f_count--;
            current->filp[fd] = NULL;
            return error;
        }
    }
    f->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);
    return fd;
}

asmlinkage int sys_open(const char * filename, int flags, int mode) {
    char *tmp;
    int error;

    error = getname(filename, &tmp);
    if (error)
        return error;
    error = do_open(tmp, flags, mode);
}