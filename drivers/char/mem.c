#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/tty.h>

static int memory_open(struct inode * inode, struct file * filp) {
    return 0;
}

static struct file_operations memory_fops = {
	NULL,		/* lseek */
	NULL,		/* read */
	NULL,		/* write */
	NULL,		/* readdir */
	NULL,		/* select */
	NULL,		/* ioctl */
	NULL,		/* mmap */
	memory_open,	/* just a selector for the real open */
	NULL,		/* release */
	NULL		/* fsync */
};

long chr_dev_init(long mem_start, long mem_end) {
    if (register_chrdev(MEM_MAJOR, "mem", &memory_fops))    
        printk("unable to get major %d for memory devs\n", MEM_MAJOR);
    mem_start = tty_init(mem_start);
#ifdef CONFIG_PRINTER
    mem_start = lp_init(mem_start);
#endif

    return mem_start;
}