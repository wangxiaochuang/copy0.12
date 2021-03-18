#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/config.h>

#define REALLY_SLOW_IO
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR HD_MAJOR
#include "blk.h"

#define HD_IRQ 14

#define MAX_HD		2

struct hd_i_struct {
	unsigned int head, sect, cyl, wpcom, lzone, ctl;
};
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };
static int NR_HD = ((sizeof (hd_info))/(sizeof (struct hd_i_struct)));
#else
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

static struct hd_struct hd[MAX_HD<<6]={{0,0},};
static int hd_sizes[MAX_HD<<6] = {0, };
static int hd_blocksizes[MAX_HD<<6] = {0, };

static void hd_times_out(void) {

}

static void do_hd_request(void) {

}

static int hd_ioctl(struct inode * inode, struct file * file,
	unsigned int cmd, unsigned long arg) {
    return 0;
}

static int hd_open(struct inode * inode, struct file * filp) {
	return 0;
}

static void hd_release(struct inode * inode, struct file * file) {

}

static void hd_geninit(void);

static struct gendisk hd_gendisk = {
	MAJOR_NR,	/* Major number */	
	"hd",		/* Major name */
	6,		/* Bits to shift to get real from partition */
	1 << 6,		/* Number of partitions per real */
	MAX_HD,		/* maximum number of real */
	hd_geninit,	/* init function */
	hd,		/* hd struct */
	hd_sizes,	/* block sizes */
	0,		/* number */
	(void *) hd_info,	/* internal */
	NULL		/* next */
};

static void hd_geninit(void) {

}

static struct file_operations hd_fops = {
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* select */
	hd_ioctl,		/* ioctl */
	NULL,			/* mmap */
	hd_open,		/* open */
	hd_release,		/* release */
	block_fsync		/* fsync */
};

unsigned long hd_init(unsigned long mem_start, unsigned long mem_end) {
    if (register_blkdev(MAJOR_NR, "hd", &hd_fops)) {
        printk("Unable to get major %d for harddisk\n",MAJOR_NR);
		return mem_start;
    }
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    read_ahead[MAJOR_NR] = 8;
    hd_gendisk.next = gendisk_head;
	gendisk_head = &hd_gendisk;
	timer_table[HD_TIMER].fn = hd_times_out;
    return mem_start;
}