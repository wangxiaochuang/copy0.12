#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <linux/sched.h>

extern int tty_ioctl(int dev, int cmd, int arg);
extern int pipe_ioctl(struct m_inode *pino, int cmd, int arg);

/* 定义输入输出控制(ioctl)函数指针类型 */
typedef int (*ioctl_ptr)(int dev,int cmd,int arg);

/* 取系统中设备种数的宏 */
#define NRDEVS ((sizeof (ioctl_table))/(sizeof (ioctl_ptr)))

/* ioctl操作函数指针表 */
static ioctl_ptr ioctl_table[]={
	NULL,		/* nodev */
	NULL,		/* /dev/mem */
	NULL,		/* /dev/fd */
	NULL,		/* /dev/hd */
	tty_ioctl,	/* /dev/ttyx */
	tty_ioctl,	/* /dev/tty */
	NULL,		/* /dev/lp */
	NULL};		/* named pipes */

int sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg) {
    struct file *filp;
    int dev, mode;
    if (fd >= NR_OPEN || !(filp = current->filp[fd])) {
        return -EBADF;
    }
    if (filp->f_inode->i_pipe) {
        // 是否有权限
        return (filp->f_mode & 1) ? pipe_ioctl(filp->f_inode, cmd, arg) : -EBADF;
    }
    mode = filp->f_inode->i_mode;
    if (!S_ISCHR(mode) && !S_ISBLK(mode)) {
        return -EINVAL;
    }
    // 字符设备和块设备的设备号放在i_zone[0]
    dev = filp->f_inode->i_zone[0];
    if (MAJOR(dev) >= NRDEVS) {
        return -ENODEV;
    }
    if (!ioctl_table[MAJOR(dev)]) {
        return -ENOTTY;
    }
    return ioctl_table[MAJOR(dev)](dev, cmd, arg);
}