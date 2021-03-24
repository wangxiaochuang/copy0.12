#include <asm/segment.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/locks.h>

#include <linux/fs.h>
#include <linux/minix_fs.h>


#define blocksize BLOCK_SIZE
#define addr_per_block 512

int minix_sync_file(struct inode * inode, struct file * file) {
    return 0;
}