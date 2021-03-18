#ifndef _LINUX_GENHD_H
#define _LINUX_GENHD_H

struct hd_struct {
	long start_sect;
	long nr_sects;
};

struct gendisk {
	int major;			/* major number of driver */
	char *major_name;		/* name of major driver */
	int minor_shift;		/* number of times minor is shifted to
					   get real minor */
	int max_p;			/* maximum partitions per device */
	int max_nr;			/* maximum number of real devices */

	void (*init)(void);		/* Initialization called before we do our thing */
	struct hd_struct *part;		/* partition table */
	int *sizes;			/* size of device in blocks */
	int nr_real;			/* number of real devices */

	void *real_devices;		/* internal use */
	struct gendisk *next;
};

extern struct gendisk *gendisk_head;

#endif