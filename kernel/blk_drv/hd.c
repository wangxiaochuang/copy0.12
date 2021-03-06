#include <linux/config.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>

#define MAJOR_NR 3
#include "blk.h"

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define MAX_ERRORS	7
#define MAX_HD		2

static void recal_intr(void);
static void bad_rw_intr(void);

static int recalibrate = 0;
static int reset = 0;

struct hd_i_struct {
	int head, sect, cyl, wpcom, lzone, ctl;
};

#ifdef HD_TYPE
    struct hd_i_struct hd_info[] = { HD_TYPE };
    #define NR_HD ((sizeof (hd_info)) / (sizeof (struct hd_i_struct)))
#else
    struct hd_i_struct hd_info[] = { {0,0,0,0,0,0}, {0,0,0,0,0,0} };
    static int NR_HD = 0;
#endif

static struct hd_struct {
    long start_sect;
    long nr_sects;
} hd[MAX_HD<<6] = {{0, 0},};

static int hd_sizes[MAX_HD<<6] = {0, };

#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr))

#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr))

extern void hd_interrupt(void);
extern void rd_load(void);

static unsigned int current_minor;

static void extended_partition(unsigned int dev) {
	struct buffer_head *bh;
	struct partition *p;
	unsigned long first_sector, this_sector;

	first_sector = hd[MINOR(dev)].start_sect;
	this_sector = first_sector;

	while (1) {
		if ((current_minor & 0x3f) >= 60)
			return;
		if (!(bh = bread(dev, 0))) {
			printk("Unable to read partition table of device %04x\n",dev);
			return;
		}
		bh->b_dirt = 0;
		bh->b_uptodate = 0;
		if (*(unsigned short *) (bh->b_data+510) == 0xAA55) {
			p = 0x1BE + (void *) bh->b_data;
			if (p->sys_ind == EXTENDED_PARTITION || !(hd[current_minor].nr_sects = p->nr_sects))
				goto done;
			hd[current_minor].start_sect = this_sector + p->start_sect;
			current_minor++;
			p++;
			if (p->sys_ind != EXTENDED_PARTITION || !(hd[current_minor].nr_sects = p->nr_sects))
				goto done;
			hd[current_minor].start_sect = first_sector + p->start_sect;
			this_sector = first_sector + p->start_sect;
			dev = 0x0300 | current_minor;
			brelse(bh);
		} else
			goto done;
	}
done:
	brelse(bh);
}

/**
 * current_minor: 硬盘的第一个分区
 * dev: 对应硬盘的第0个位置，1-4分区
 **/
static void check_partition(unsigned int dev) {
	int i, minor = current_minor;
	struct buffer_head *bh;
	struct partition *p;
	unsigned long first_sector;

	first_sector = hd[MINOR(dev)].start_sect;
	if (!(bh = bread(dev, 0))) {
		printk("Unable to read partition table of device %04x\n",dev);
		return;
	}
	printk("Drive %d:\n\r", minor >> 6);
	current_minor += 4;		/* first "extra" minor */
	if (*(unsigned short *) (bh->b_data+510) == 0xAA55) {
		p = 0x1BE + (void *)bh->b_data;		// 分区表放启动盘的0x1BE开始的位置的
		// 4个主分区信息存放在启动盘的后面位置
		for (i = 1; i <= 4; minor++, i++, p++) {
			if (!(hd[minor].nr_sects = p->nr_sects))
				continue;
			hd[minor].start_sect = first_sector + p->start_sect;
			
			if ((current_minor & 0x3f) >= 60)
				continue;
			if (p->sys_ind == EXTENDED_PARTITION) {
				extended_partition(0x0300 | minor);
			}
		}
		/* check for Disk Manager partition table */
		if (*(unsigned short *) (bh->b_data+0xfc) == 0x55AA) {
			p = 0x1BE + (void *) bh->b_data;
			for (i = 4; i < 16; i++, current_minor++) {
				p--;
				if ((current_minor & 0x3f) >= 60)
					break;
				if (!(p->start_sect && p->nr_sects))
					continue;
				hd[current_minor].start_sect = p->start_sect;
				hd[current_minor].nr_sects = p->nr_sects;
				printk(" DM part %d start %d size %d end %d\n\r",
				       current_minor,
				       hd[current_minor].start_sect, 
				       hd[current_minor].nr_sects,
				       hd[current_minor].start_sect + 
				       hd[current_minor].nr_sects);
			}
		}
	} else 
		printk("Bad partition table on dev %04x\n",dev);
	brelse(bh);
}

int sys_setup(void * BIOS) {
    static int callable = 1;
    int i, drive;
    unsigned char cmos_disks;
    if (!callable)
        return -1;
    callable = 0;
#ifndef HD_TYPE
    for (drive = 0; drive < 2; drive++) {
		hd_info[drive].cyl = *(unsigned short *) BIOS;
		hd_info[drive].head = *(unsigned char *) (2+BIOS);
		hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);
		hd_info[drive].ctl = *(unsigned char *) (8+BIOS);
		hd_info[drive].lzone = *(unsigned short *) (12+BIOS);
		hd_info[drive].sect = *(unsigned char *) (14+BIOS);
		BIOS += 16;
    }
    /* 看是不是兼容AT硬盘 */
    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
        if (cmos_disks & 0x0f)
            NR_HD = 2;
        else
            NR_HD = 1;
    else
        NR_HD = 0;
#endif
	for (i = 0; i < (MAX_HD<<6); i++) {
		hd[i].start_sect = 0;
		hd[i].nr_sects = 0;
	}
    for (i = 0; i < NR_HD; i++) {
        hd[i<<6].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
    }

    for (drive = 0; drive < NR_HD; drive++) {
		current_minor = 1 + (drive << 6);
		check_partition(0x0300+(drive<<6));
    }
	for (i = 0; i < (MAX_HD<<6); i++)
		hd_sizes[i] = hd[i].nr_sects >> 1;
	blk_size[MAJOR_NR] = hd_sizes;
	if (NR_HD)
		printk("Partition table%s ok.\n\r",(NR_HD > 1) ? "s" : "");
	rd_load();
	mount_root();
	return 0;
}

static int controller_ready(void) {
	int retries = 100000;

	while (--retries && (inb_p(HD_STATUS) & 0x80));
	return (retries);
}

static int win_result(void) {
	int i=inb_p(HD_STATUS);

	if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
		== (READY_STAT | SEEK_STAT))
		return(0); /* ok */
	if (i & 1) i = inb(HD_ERROR);
	return (1);
}

static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
		unsigned int head,unsigned int cyl,unsigned int cmd,
		void (*intr_addr)(void))
{
	register int port asm("dx");

	if (drive > 1 || head > 15)
		panic("Trying to write bad sector");
	if (!controller_ready())
		panic("HD controller not ready");
	SET_INTR(intr_addr);
	outb_p(hd_info[drive].ctl, HD_CMD);
	port = HD_DATA;
	outb_p(hd_info[drive].wpcom>>2, ++port);
	outb_p(nsect, ++port);
	outb_p(sect, ++port);
	outb_p(cyl, ++port);
	outb_p(cyl >> 8, ++port);
	outb_p(0xA0|(drive<<4)|head, ++port);
	outb(cmd, ++port);
}

static int drive_busy(void) {
    unsigned int i;
    unsigned char c;

    for (i = 0; i < 50000; i++) {
        c = inb_p(HD_STATUS);
        c &= (BUSY_STAT | READY_STAT | SEEK_STAT);
        if (c == (READY_STAT | SEEK_STAT))
            return 0;
    }
    printk("HD controller times out\n\r");
    return(1);
}

static void reset_controller(void) {
    int i;
    outb(4, HD_CMD);
    for (i = 0; i < 10000; i++) nop();
    outb(hd_info[0].ctl & 0x0f, HD_CMD);
    if (drive_busy())
        printk("HD-controller still busy\n\r");
    if ((i = inb(HD_ERROR)) != 1)
        printk("HD-controller reset failed: %02x\n\r",i);
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
    } else {
        do_hd_request();
    }
}
void unexpected_hd_interrupt(void) {
	printk("Unexpected HD interrupt\n\r");
	reset = 1;
	do_hd_request();
}

static void bad_rw_intr(void) {
	if (++CURRENT->errors >= MAX_ERRORS)
		end_request(0);
	if (CURRENT->errors > MAX_ERRORS / 2)
		reset = 1;
}

static void read_intr(void) {
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	port_read(HD_DATA, CURRENT->buffer, 256);
	CURRENT->errors = 0;
	CURRENT->buffer += 512;
	CURRENT->sector++;
	if (--CURRENT->nr_sectors) {
		SET_INTR(&read_intr);
		return;
	}
	end_request(1);
	do_hd_request();
}

static void write_intr(void) {
	if (win_result()) {
		bad_rw_intr();
		do_hd_request();
		return;
	}
	if (--CURRENT->nr_sectors) {
		CURRENT->sector++;
		CURRENT->buffer += 512;
		SET_INTR(&write_intr);
		port_write(HD_DATA,CURRENT->buffer,256);
		return;
	}
	end_request(1);
	do_hd_request();
}

static void recal_intr(void) {
	if (win_result())
		bad_rw_intr();
	do_hd_request();
}

void hd_times_out(void) {
	do_hd = NULL;
	reset = 1;
	if (!CURRENT)
		return;
	printk("HD timeout\n\r");
	cli();
	if (++CURRENT->errors >= MAX_ERRORS)
		end_request(0);
	do_hd_request();
}

void do_hd_request(void) {
    int i,r;
	unsigned int block,dev;
	unsigned int sec,head,cyl;
	unsigned int nsect;

    INIT_REQUEST;
    dev = MINOR(CURRENT->dev);
    block = CURRENT->sector;
    // dev: 0, block: 0
	if (dev >= 5 * NR_HD || block+2 > hd[dev].nr_sects) {
		end_request(0);
		goto repeat;
	}
    block += hd[dev].start_sect;
    dev /= 5;
	__asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
		"r" (hd_info[dev].sect));
	__asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
		"r" (hd_info[dev].head));
    sec++;
    nsect = CURRENT->nr_sectors;
    if (reset) {
        recalibrate = 1;
        reset_hd();
        return;
    }
    // 重新校正，让处于任何地方的磁头移动到0柱面
    if (recalibrate) {
        recalibrate = 0;
		hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
			WIN_RESTORE,&recal_intr);
        return;
    }
	if (CURRENT->cmd == WRITE) {
		hd_out(dev, nsect, sec, head, cyl, WIN_WRITE, &write_intr);
		for(i = 0 ; i < 10000 && !(r = inb_p(HD_STATUS) & DRQ_STAT) ; i++)
			/* nothing */ ;
		if (!r) {
			bad_rw_intr();
			goto repeat;
		}
		port_write(HD_DATA,CURRENT->buffer,256);
	} else if (CURRENT->cmd == READ) {
		hd_out(dev, nsect, sec, head, cyl, WIN_READ, &read_intr);
	} else
		panic("unknown hd-command");
}

void hd_init(void) {
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    set_intr_gate(0x2E, &hd_interrupt);
    outb_p(inb_p(0x21) & 0xfb, 0x21);
	outb(inb_p(0xA1) & 0xbf, 0xA1);
	timer_table[HD_TIMER].fn = hd_times_out;
}