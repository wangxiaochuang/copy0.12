#include <linux/config.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>

struct gendisk *gendisk_head = NULL;

static int current_minor = 0;
extern int *blk_size[];

static void check_partition(struct gendisk *hd, unsigned int dev) {
    static int first_time = 1;
	int i, minor = current_minor;
	struct buffer_head *bh;
	struct partition *p;
	unsigned long first_sector;
	int mask = (1 << hd->minor_shift) - 1;

	if (first_time)
		printk("Partition check:\n");
	first_time = 0;
    first_sector = hd->part[MINOR(dev)].start_sect;
    if (!(bh = bread(dev, 0, 1024))) {
        printk("  unable to read partition table of device %04x\n",dev);
		return;
    }
    printk("  %s%c:", hd->major_name, 'a'+(minor >> hd->minor_shift));
}

static void setup_dev(struct gendisk *dev) {
    int i;
	int j = dev->max_nr * dev->max_p;
	int major = dev->major << 8;
	int drive;

    for (i = 0; i < j; i++) {
        dev->part[i].start_sect = 0;
        dev->part[i].nr_sects = 0;
    }
    dev->init();
    for (drive = 0; drive < dev->nr_real; drive++) {
        current_minor = 1 + (drive << dev->minor_shift);
        check_partition(dev, major + (drive << dev->minor_shift));
    }
    // 根据扇区数算出逻辑块数，存放在sizes里
    for (i = 0; i < j; i++)
        dev->sizes[i] = dev->part[i].nr_sects >> (BLOCK_SIZE_BITS - 9);
    blk_size[dev->major] = dev->sizes;
}

asmlinkage int sys_setup(void * BIOS) {
    static int callable = 1;
    struct gendisk *p;
    int nr = 0;

    if (!callable)
        return -1;
    callable = 0;

    for (p = gendisk_head; p; p = p->next) {
        setup_dev(p);
        nr += p->nr_real;
    }

    return 0;
}