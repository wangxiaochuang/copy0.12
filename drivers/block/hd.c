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

static inline unsigned char CMOS_READ(unsigned char addr) {
	outb_p(addr, 0x70);
	return inb_p(0x71);
}

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

void unexpected_hd_interrupt(void) {
	sti();
	printk(KERN_DEBUG "Unexpected HD interrupt\n");
	SET_TIMER;
}

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

static void hd_interrupt(int unused) {
	void (*handler)(void) = DEVICE_INTR;

	DEVICE_INTR = NULL;
	timer_active &= ~(1<<HD_TIMER);
	if (!handler)
		handler = unexpected_hd_interrupt;
	handler();
	sti();
}

static struct sigaction hd_sigaction = {
	hd_interrupt,
	0,
	SA_INTERRUPT,
	NULL
};

static void hd_geninit(void) {
	int drive, i;
	extern struct drive_info drive_info;
	unsigned char *BIOS = (unsigned char *) &drive_info;
	int cmos_disks;

	if (!NR_HD) {
		for (drive = 0; drive < MAX_HD; drive++) {
			hd_info[drive].cyl = *(unsigned short *) BIOS;
			hd_info[drive].head = *(2 + BIOS);
			hd_info[drive].wpcom = *(unsigned short *) (5 + BIOS);
			hd_info[drive].ctl = *(8 + BIOS);
			hd_info[drive].lzone = *(unsigned short *) (12 + BIOS);
			hd_info[drive].sect = *(14 + BIOS);
			BIOS += 16;
		}
		if ((cmos_disks = CMOS_READ(0x12)) & 0xf0) {
			if (cmos_disks & 0x0f)
				NR_HD = 2;
			else
				NR_HD = 1;
		}
	}
	i = NR_HD;
	while (i-- > 0) {
		hd[i<<6].nr_sects = 0;
		if (hd_info[i].head > 16) {
			printk("hd.c: ST-506 interface disk with more than 16 heads detected,\n");
			printk("  probably due to non-standard sector translation. Giving up.\n");
			printk("  (disk %d: cyl=%d, sect=%d, head=%d)\n", i,
				hd_info[i].cyl,
				hd_info[i].sect,
				hd_info[i].head);
			if (i + 1 == NR_HD)
				NR_HD--;
			continue;
		}
		hd[i<<6].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
	}
	if (NR_HD) {
		if (irqaction(HD_IRQ, &hd_sigaction)) {
			printk("hd.c: unable to get IRQ%d for the harddisk driver\n",HD_IRQ);
			NR_HD = 0;
		}
	}
	hd_gendisk.nr_real = NR_HD;

	for (i = 0; i < (MAX_HD << 6); i++)
		hd_blocksizes[i] = 1024;
	blksize_size[MAJOR_NR] = hd_blocksizes;
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