#ifndef _BLK_H
#define _BLK_H

#include <linux/major.h>
#include <linux/sched.h>

#define NR_REQUEST	64

struct request {
	int dev;		/* -1 if no request */
	int cmd;		/* READ or WRITE */
	int errors;
	unsigned long sector;
	unsigned long nr_sectors;
	unsigned long current_nr_sectors;
	char * buffer;
	struct task_struct * waiting;
	struct buffer_head * bh;
	struct buffer_head * bhtail;
	struct request * next;
};

#define IN_ORDER(s1,s2) \
((s1)->cmd < (s2)->cmd || ((s1)->cmd == (s2)->cmd && \
((s1)->dev < (s2)->dev || (((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector)))))

struct blk_dev_struct {
	void (*request_fn)(void);
	struct request * current_request;
};

struct sec_size {
	unsigned block_size;
	unsigned block_size_bits;
};

extern struct blk_dev_struct blk_dev[MAX_BLKDEV];

extern unsigned long hd_init(unsigned long mem_start, unsigned long mem_end);

#ifdef MAJOR_NR

#if (MAJOR_NR == MEM_MAJOR)
#elif (MAJOR_NR == FLOPPY_MAJOR)
#elif (MAJOR_NR == HD_MAJOR)

/* harddisk: timeout is 6 seconds.. */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_TIMEOUT HD_TIMER
#define TIMEOUT_VALUE 600
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)>>6)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#elif (MAJOR_NR == SCSI_DISK_MAJOR)
#elif (MAJOR_NR == SCSI_TAPE_MAJOR)
#elif (MAJOR_NR == SCSI_CDROM_MAJOR)
#elif (MAJOR_NR == XT_DISK_MAJOR)
#elif (MAJOR_NR == CDU31A_CDROM_MAJOR)
#elif (MAJOR_NR == MITSUMI_CDROM_MAJOR)
#elif (MAJOR_NR == MATSUSHITA_CDROM_MAJOR)
#else

#error "unknown blk device"

#endif

#endif

#endif