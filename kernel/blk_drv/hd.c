#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/io.h>

#define MAJOR_NR 3
#include "blk.h"

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define MAX_HD		2

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
} hd[5 * MAX_HD] = {{0, 0},};

extern void hd_interrupt(void);

int sys_setup(void * BIOS) {
    static int callable = 1;
    int i, drive;
    unsigned char cmos_disks;
    struct buffer_head * bh;
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
    if (hd_info[1].cyl)
        NR_HD = 2;
    else
        NR_HD = 1;
#endif
    for (i = 0; i < NR_HD; i++) {
        hd[i * 5].start_sect = 0;
        hd[i * 5].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
    }
    /* 看是不是兼容AT硬盘 */
    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0)
        if (cmos_disks & 0x0f)
            NR_HD = 2;
        else
            NR_HD = 1;
    else
        NR_HD = 0;
    for (i = NR_HD ; i < 2 ; i++) {
		hd[i*5].start_sect = 0;
		hd[i*5].nr_sects = 0;
	}

    for (drive = 0; drive < NR_HD; drive++) {
        if (!(bh = bread(0x300 + drive * 5, 0))) {
            panic("Unable to read partition table of drive %d\n\r");
        }
        panic("aaaaa: 0x%02x", bh->b_data[510]);
    }
}

void unexpected_hd_interrupt(void) {
	printk("Unexpected HD interrupt\n\r");
	reset = 1;
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
	if (dev >= 5 * NR_HD || block+2 > hd[dev].nr_sects) {
		end_request(0);
		goto repeat;
	}
}

void hd_init(void) {
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    set_intr_gate(0x2E, &hd_interrupt);
    outb_p(inb_p(0x21) & 0xfb, 0x21);
	outb(inb_p(0xA1) & 0xbf, 0xA1);
}