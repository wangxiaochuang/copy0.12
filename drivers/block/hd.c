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

#define	HD_DELAY	0

#define MAX_ERRORS     16
#define RESET_FREQ      8
#define RECAL_FREQ      4
#define MAX_HD		2

static void recal_intr(void);
static void bad_rw_intr(void);

static char recalibrate[ MAX_HD ] = { 0, };

static int reset = 0;
static int hd_error = 0;

#if (HD_DELAY > 0)
unsigned long last_req, read_timer();
#endif

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

static int win_result(void) {
	int i = inb_p(HD_STATUS);

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT)) {
	        hd_error = 0;
		return 0; /* ok */
	}
	printk("HD: win_result: status = 0x%02x\n",i);
	if (i & 1) {
		hd_error = inb(HD_ERROR);
		printk("HD: win_result: error = 0x%02x\n",hd_error);
	}
	return 1;
}

static int controller_busy(void);
static int status_ok(void);

static int controller_ready(unsigned int drive, unsigned int head) {
	int retry = 100;
	do {
		if (controller_busy() & BUSY_STAT)
			return 0;
		outb_p(0xA0 | (drive << 4) | head, HD_CURRENT);
		if (status_ok())
			return 1;
	} while (--retry);
	return 0;
}

static int status_ok(void) {
	unsigned char status = inb_p(HD_STATUS);

	if (status & BUSY_STAT)
		return 1;
	if (status & WRERR_STAT)
		return 0;
	if (!(status & READY_STAT))
		return 0;
	if (!(status & SEEK_STAT))
		return 0;
	return 1;
}

static int controller_busy(void) {
	int retries = 100000;
	unsigned char status;

	do {
		status = inb_p(HD_STATUS);
	} while ((status & BUSY_STAT) && --retries);
	return status;
}

static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void)) {
    unsigned short port;
	if (drive>1 || head>15)
		panic("Trying to write bad sector");
	if (reset)
		return;
	if (!controller_ready(drive, head)) {
		reset = 1;
		return;
	}
	SET_INTR(intr_addr);
	outb_p(hd_info[drive].ctl, HD_CMD);
	port = HD_DATA;
	outb_p(hd_info[drive].wpcom>>2, ++port);
	outb_p(nsect, ++port);
	outb_p(sect, ++port);
	outb_p(cyl, ++port);
	outb_p(cyl>>8, ++port);
	outb_p(0xA0|(drive<<4)|head, ++port);
	outb_p(cmd, ++port);
}

static int drive_busy(void) {
	unsigned int i;
	unsigned char c;

	for (i = 0; i < 500000; i++) {
		c = inb_p(HD_STATUS);
		c &= (BUSY_STAT | READY_STAT | SEEK_STAT);
		if (c == (READY_STAT | SEEK_STAT))
			return 0;
	}
	printk("HD controller times out, status = 0x%02x\n",c);
	return 1;
}

static void reset_controller(void) {
	int	i;

	printk(KERN_DEBUG "HD-controller reset\n");
	outb_p(4, HD_CMD);
	for (i = 0; i < 1000; i++) nop();
	outb(hd_info[0].ctl & 0x0f, HD_CMD);
	if (drive_busy())
		printk("HD-controller still busy\n");
	if ((hd_error = inb(HD_ERROR)) != 1)
		printk("HD-controller reset failed: %02x\n",hd_error);
}

static void reset_hd(void) {
	static int i;
repeat:
	if (reset) {
		reset = 0;
		i = -1;
		reset_controller();
	} else if (win_result()) {
		bad_rw_intr();
		if (reset)
			goto repeat;
	}
	i++;
	if (i < NR_HD) {
		hd_out(i, hd_info[i].sect, hd_info[i].sect, hd_info[i].head - 1,
			hd_info[i].cyl, WIN_SPECIFY, &reset_hd);
		if (reset)
			goto repeat;
	} else
		do_hd_request();
}

void unexpected_hd_interrupt(void) {
	sti();
	printk(KERN_DEBUG "Unexpected HD interrupt\n");
	SET_TIMER;
}

static void bad_rw_intr(void) {
	int dev;
	if (!CURRENT)
		return;
	dev = MINOR(CURRENT->dev) >> 6;
	if (++CURRENT->errors >= MAX_ERRORS || (hd_error & BBD_ERR)) {
		end_request(0);
		recalibrate[dev] = 1;
	} else if (CURRENT->errors % RESET_FREQ == 0) {
		reset = 1;
	} else if ((hd_error & TRK0_ERR) || CURRENT->errors % RECAL_FREQ == 0) {
		recalibrate[dev] = 1;
	}
}

static inline int wait_DRQ(void) {
	int retries = 100000;
	while(--retries > 0)
		if (inb_p(HD_STATUS) & DRQ_STAT)
			return 0;
	return -1;
}

#define STAT_MASK (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT)
#define STAT_OK (READY_STAT | SEEK_STAT)

static void read_intr(void) {
	int i;
	int retries = 100000;

	do {
		i = (unsigned) inb_p(HD_STATUS);
		if (i & BUSY_STAT)
			continue;
		if ((i & STAT_MASK) != STAT_OK)
			break;
		if (i & DRQ_STAT)
			goto ok_to_read;
	} while (--retries > 0);
	sti();
	printk("HD: read_intr: status = 0x%02x\n",i);
	if (i & ERR_STAT) {
		hd_error = (unsigned) inb(HD_ERROR);
		printk("HD: read_intr: error = 0x%02x\n",hd_error);
	}
	bad_rw_intr();
	cli();
	do_hd_request();
	return;
ok_to_read:
	insw(HD_DATA, CURRENT->buffer, 256);
	CURRENT->errors = 0;
	CURRENT->buffer += 512;
	CURRENT->sector++;
	i = --CURRENT->nr_sectors;
	--CURRENT->current_nr_sectors;
	if (!i || (CURRENT->bh && !SUBSECTOR(i)))
		end_request(1);
	if (i > 0) {
		SET_INTR(&read_intr);
		sti();
		return;
	}
	(void) inb_p(HD_STATUS);
#if (HD_DELAY > 0)
	last_req = read_timer();
#endif
	do_hd_request();
	return;
}

static void write_intr(void) {
	int i;
	int retries = 100000;

	do {
		i = (unsigned) inb_p(HD_STATUS);
		if (i & BUSY_STAT)
			continue;
		if ((i & STAT_MASK) != STAT_OK)
			break;
		if ((CURRENT->nr_sectors <= 1 || (i & DRQ_STAT)))
			goto ok_to_write;
	} while (--retries > 0);
	sti();
	printk("HD: write_intr: status = 0x%02x\n",i);
	if (i & ERR_STAT) {
		hd_error = (unsigned) inb(HD_ERROR);
		printk("HD: write_intr: error = 0x%02x\n",hd_error);
	}
	bad_rw_intr();
	cli();
	do_hd_request();
	return;
ok_to_write:
	CURRENT->sector++;
	i = --CURRENT->nr_sectors;
	--CURRENT->current_nr_sectors;
	CURRENT->buffer += 512;
	if (!i || (CURRENT->bh && !SUBSECTOR(i)))
		end_request(1);
	if (i > 0) {
		SET_INTR(&write_intr);
		outsw(HD_DATA, CURRENT->buffer, 256);
		sti();
	} else {
#if (HD_DELAY > 0)
		last_req = read_timer();
#endif
		do_hd_request();
	}
	return;
}

static void recal_intr(void) {
	if (win_result())
		bad_rw_intr();
	do_hd_request();
}

static void hd_times_out(void) {
	DEVICE_INTR = NULL;
	sti();
	reset = 1;
	if (!CURRENT)
		return;
	printk(KERN_DEBUG "HD timeout\n");
	cli();
	if (++CURRENT->errors >= MAX_ERRORS) {
		end_request(0);
	}
	do_hd_request();
}

static void do_hd_request(void) {
	unsigned int block, dev;
	unsigned int sec, head, cyl, track;
	unsigned int nsect;

	if (CURRENT && CURRENT->dev < 0) return;
	if (DEVICE_INTR)
		return;
repeat:
	timer_active &= ~(1 << HD_TIMER);
	sti();
	INIT_REQUEST;
	dev = MINOR(CURRENT->dev);
	block = CURRENT->sector;
	nsect = CURRENT->nr_sectors;
	if (dev >= (NR_HD << 6) || block >= hd[dev].nr_sects) {
		end_request(0);
		goto repeat;
	}
	// 对每个分区的读操作都是从0块开始的，所以这里要加上该分区的开始扇区号
	block += hd[dev].start_sect;
	dev >>= 6;
	sec = block % hd_info[dev].sect + 1;
	track = block / hd_info[dev].sect;
	head = track % hd_info[dev].head;
	cyl = track / hd_info[dev].head;
	cli();
	if (reset) {
		int i;
		for (i = 0; i < NR_HD; i++)
			recalibrate[i] = 1;
		reset_hd();
		sti();
		return;
	}
	if (recalibrate[dev]) {
		recalibrate[dev] = 0;
		hd_out(dev, hd_info[dev].sect, 0, 0, 0, WIN_RESTORE, &recal_intr);
		if (reset)
			goto repeat;
		sti();
		return;
	}
	if (CURRENT->cmd == WRITE) {
		hd_out(dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr);
		if (reset)
			goto repeat;
		if (wait_DRQ()) {
			printk("HD: do_hd_request: no DRQ\n");
			bad_rw_intr();
			goto repeat;
		}
		outsw(HD_DATA, CURRENT->buffer, 256);
		sti();
		return;
	}
	if (CURRENT->cmd == READ) {
		hd_out(dev, nsect, sec, head, cyl, WIN_READ, &read_intr);
		if (reset)
			goto repeat;
		sti();
		return;
	}
	panic("unknown hd-command");
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