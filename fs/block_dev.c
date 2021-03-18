#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

int block_write(struct inode * inode, struct file * filp, char * buf, int count) {
    return 0;
}

int block_read(struct inode * inode, struct file * filp, char * buf, int count) {
    return 0;
}

int block_fsync(struct inode *inode, struct file *filp) {
    return 0;
}