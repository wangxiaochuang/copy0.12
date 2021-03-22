#define REALLY_SLOW_IO
#define FLOPPY_IRQ 6
#define FLOPPY_DMA 2

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fdreg.h>
#include <linux/fd.h>
#include <linux/errno.h>

#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR FLOPPY_MAJOR
#include "blk.h"

static int initial_reset_flag = 0;
static int need_configure = 1;
static int recalibrate = 0;
static int reset = 0;
static int recover = 0;
static int seek = 0;

static unsigned char current_DOR = 0x0C;
static unsigned char running = 0;

#define TYPE(x) ((x)>>2)
#define DRIVE(x) ((x)&0x03)

#define MAX_ERRORS 12

#define MAX_DISK_SIZE 1440

#define MAX_BUFFER_SECTORS 18

#define LAST_DMA_ADDR	(0x100000 - BLOCK_SIZE)

#define MAX_REPLIES 7
static unsigned char reply_buffer[MAX_REPLIES];
#define ST0 (reply_buffer[0])
#define ST1 (reply_buffer[1])
#define ST2 (reply_buffer[2])
#define ST3 (reply_buffer[3])

static struct floppy_struct floppy_type[] = {
	{    0, 0,0, 0,0,0x00,0x00,0x00,0x00,NULL },	/* no testing */
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,NULL },	/* 360kB PC diskettes */
	{ 2400,15,2,80,0,0x1B,0x00,0xDF,0x54,NULL },	/* 1.2 MB AT-diskettes */
	{  720, 9,2,40,1,0x2A,0x02,0xDF,0x50,NULL },	/* 360kB in 720kB drive */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,NULL },	/* 3.5" 720kB diskette */
	{  720, 9,2,40,1,0x23,0x01,0xDF,0x50,NULL },	/* 360kB in 1.2MB drive */
	{ 1440, 9,2,80,0,0x23,0x01,0xDF,0x50,NULL },	/* 720kB in 1.2MB drive */
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,NULL },	/* 1.44MB diskette */
};

static struct floppy_struct floppy_types[] = {
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{  720, 9,2,40,0,0x2A,0x02,0xDF,0x50,"360k/PC" }, /* 360kB PC diskettes */
	{ 2400,15,2,80,0,0x1B,0x00,0xDF,0x54,"1.2M" },	  /* 1.2 MB AT-diskettes */
	{  720, 9,2,40,1,0x23,0x01,0xDF,0x50,"360k/AT" }, /* 360kB in 1.2MB drive */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k" },	  /* 3.5" 720kB diskette */
	{ 2880,18,2,80,0,0x1B,0x00,0xCF,0x6C,"1.44M" },	  /* 1.44MB diskette */
	{ 1440, 9,2,80,0,0x2A,0x02,0xDF,0x50,"720k/AT" }, /* 3.5" 720kB diskette */
};

struct floppy_struct *current_type[4] = { NULL, NULL, NULL, NULL };

struct floppy_struct *base_type[4];

static int floppy_sizes[] ={
	MAX_DISK_SIZE, MAX_DISK_SIZE, MAX_DISK_SIZE, MAX_DISK_SIZE,
	 360, 360 ,360, 360,
	1200,1200,1200,1200,
	 360, 360, 360, 360,
	 720, 720, 720, 720,
	 360, 360, 360, 360,
	 720, 720, 720, 720,
	1440,1440,1440,1440
};

static void redo_fd_request(void);

#define NO_TRACK 255

static int read_track = 0;	/* flag to indicate if we want to read entire track */
static int buffer_track = -1;
static int buffer_drive = -1;
static int cur_spec1 = -1;
static int cur_rate = -1;
static struct floppy_struct * floppy = floppy_type;
static unsigned char current_drive = 255;
static unsigned char sector = 0;
static unsigned char head = 0;
static unsigned char track = 0;
static unsigned char seek_track = 0;
static unsigned char current_track = NO_TRACK;
static unsigned char command = 0;
static unsigned char fdc_version = FDC_TYPE_STD;	/* FDC version code */

static void floppy_ready(void);

static void floppy_on(unsigned int nr) {

}

static void floppy_off(unsigned int nr) {

}

static void output_byte(char byte) {
    int counter;
    unsigned char status;

    if (reset)
        return;
    for (counter = 0; counter < 10000; counter++) {
        status = inb_p(FD_STATUS) & (STATUS_READY | STATUS_DIR);
        if (status == STATUS_READY) {
            outb(byte, FD_DATA);
            return;
        }
    }
    current_track = NO_TRACK;
    reset = 1;
    printk("Unable to send byte to FDC\n");
}

static int result(void) {
    int i = 0, counter, status;

    if (reset)
        return -1;
    for (counter = 0; counter < 10000; counter++) {
        status = inb_p(FD_STATUS) & (STATUS_DIR | STATUS_READY | STATUS_BUSY);
        if (status == STATUS_READY) {
            return i;
        }
        if (status == (STATUS_DIR | STATUS_READY | STATUS_BUSY)) {
            if (i >= MAX_REPLIES) {
                printk("floppy_stat reply overrun\n");
                break;
            }
            reply_buffer[i++] = inb_p(FD_DATA);
        }
    }
    reset = 1;
    current_track = NO_TRACK;
    printk("Getstatus times out\n");
    return -1;
}

static void configure_fdc_mode(void) {

}

static void recalibrate_floppy(void) {

}

static void reset_interrupt(void) {
    short i;

    for (i = 0; i < 4; i++) {
        output_byte(FD_SENSEI);
        (void) result();
    }
    output_byte(FD_SPECIFY);
    output_byte(cur_spec1);
    output_byte(6);
    configure_fdc_mode();
    if (initial_reset_flag) {
        initial_reset_flag = 0;
		recalibrate = 1;
		reset = 0;
		return;
    }
    if (!recover)
		redo_fd_request();
	else {
		recalibrate_floppy();
		recover = 0;
	}
}

static void reset_floppy(void) {
    int i;

    do_floppy = reset_interrupt;
    reset = 0;
    current_track = NO_TRACK;
    cur_spec1 = -1;
    cur_rate = -1;
    recalibrate = 1;
    need_configure = 1;
    if (!initial_reset_flag)
        printk("Reset-floppy called\n");
    cli();
    outb_p(current_DOR & ~0x04, FD_DOR);
    for (i = 0; i < 1000; i++)
        __asm__("nop");
    outb(current_DOR, FD_DOR);
    sti();
}

static void floppy_shutdown(void) {

}

static void redo_fd_request(void) {
    return;
}

void do_fd_request(void) {

}

static int fd_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
    unsigned long param) {
    return 0;
}

#define CMOS_READ(addr) ({ \
outb_p(addr,0x70); \
inb_p(0x71); \
})

static struct floppy_struct *find_base(int drive,int code) {
    struct floppy_struct *base;

    if (code > 0 && code < 5) {
        base = &floppy_types[(code-1)*2];
        printk("fd%d is %s",drive,base->name);
		return base;
    }
    printk("fd%d is unknown type %d",drive,code);
	return NULL;
}

static void config_types(void) {
    printk("Floppy drive(s): ");
    base_type[0] = find_base(0, (CMOS_READ(0x10) >> 4) & 15);
    if (((CMOS_READ(0x14) >> 6) & 1) == 0)
        base_type[1] = NULL;
    else {
        printk(", ");
        base_type[1] = find_base(1, CMOS_READ(0x10) & 15);
    }
    base_type[2] = base_type[3] = NULL;
    printk("\n");
}

static int floppy_open(struct inode * inode, struct file * filp) {
    return 0;
}

static void floppy_release(struct inode * inode, struct file * filp) {

}

static struct file_operations floppy_fops = {
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* select */
	fd_ioctl,		/* ioctl */
	NULL,			/* mmap */
	floppy_open,		/* open */
	floppy_release,		/* release */
	block_fsync		/* fsync */
};

static void ignore_interrupt(void) {

}

static void floppy_interrupt(int unused) {

}

static struct sigaction floppy_sigaction = {
	floppy_interrupt,
	0,
	SA_INTERRUPT,
	NULL
};

void floppy_init(void) {
    outb(current_DOR, FD_DOR);
    if (register_blkdev(MAJOR_NR, "fd", &floppy_fops)) {
        printk("Unable to get major %d for floppy\n",MAJOR_NR);
		return;
    }

    blk_size[MAJOR_NR] = floppy_sizes;
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;

    timer_table[FLOPPY_TIMER].fn = floppy_shutdown;
    timer_active &= ~(1 << FLOPPY_TIMER);
    config_types();

    if (irqaction(FLOPPY_IRQ, &floppy_sigaction))
        printk("Unable to grab IRQ%d for the floppy driver\n", FLOPPY_IRQ);
    if (request_dma(FLOPPY_DMA))
        printk("Unable to grab DMA%d for the floppy driver\n", FLOPPY_DMA);
    DEVICE_INTR = ignore_interrupt;
    output_byte(FD_VERSION);
    if (result() != 1) {
        printk(DEVICE_NAME ": FDC failed to return version byte\n");
		fdc_version = FDC_TYPE_STD;
    } else
        fdc_version = reply_buffer[0];
    if (fdc_version != FDC_TYPE_STD)
        printk(DEVICE_NAME ": FDC version 0x%x\n", fdc_version);
    
#ifndef FDC_FIFO_UNTESTED
	fdc_version = FDC_TYPE_STD;	/* force std fdc type; can't test other. */
#endif
    if (fdc_version == FDC_TYPE_STD) {
        initial_reset_flag = 1;
        reset_floppy();
    }
}