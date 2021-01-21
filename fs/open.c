#include <errno.h>
#include <linux/sched.h>

int sys_open(const char * filename, int flag, int mode) {
    struct m_inode *inode;
    struct file *f;
    int i, fd;

    mode &= 0777 & ~current->umask;

    for (fd = 0; fd < NR_OPEN; fd++) {
        if (!current->filp[fd]) {
            break;
        }
    }
    if (fd >= NR_OPEN) {
        return -EINVAL;
    }
    current->close_on_exec &= ~(1<<fd);

    f = 0 + file_table;
    for (i = 0; i < NR_FILE; i++, f++) {
        if (!f->f_count) {
            break;
        }
    }
    if (i >= NR_FILE) {
        return -EINVAL;
    }
    (current->filp[fd] = f)->f_count++;
    if ((i = open_namei(filename, flag, mode, &inode)) < 0) {
    panic("find fail");
        current->filp[fd] = NULL;
        f->f_count = 0;
        return i;
    }
    panic("find ok");
}