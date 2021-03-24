#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/shm.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/malloc.h>

#include <asm/segment.h>
#include <asm/system.h>

int generic_mmap(struct inode * inode, struct file * file,
	unsigned long addr, size_t len, int prot, unsigned long off) {
    return 0;
}