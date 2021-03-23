#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/config.h>

#include <asm/system.h>

#include "blk.h"

static struct request all_requests[NR_REQUEST];

struct wait_queue * wait_for_request = NULL;

// 预读的快数
int read_ahead[MAX_BLKDEV] = {0, };

struct blk_dev_struct blk_dev[MAX_BLKDEV] = {
	{ NULL, NULL },		/* no_dev */
	{ NULL, NULL },		/* dev mem */
	{ NULL, NULL },		/* dev fd */
	{ NULL, NULL },		/* dev hd */
	{ NULL, NULL },		/* dev ttyx */
	{ NULL, NULL },		/* dev tty */
	{ NULL, NULL },		/* dev lp */
	{ NULL, NULL },		/* dev pipes */
	{ NULL, NULL },		/* dev sd */
	{ NULL, NULL }		/* dev st */
};

// 块设备的每个分区的扇区数
int * blk_size[MAX_BLKDEV] = { NULL, NULL, };

int * blksize_size[MAX_BLKDEV] = { NULL, NULL, };

static inline struct request * get_request(int n, int dev) {
	static struct request *prev_found = NULL, *prev_limit = NULL;
	register struct request *req, *limit;

	if (n <= 0)
		panic("get_request(%d): impossible!\n", n);
	limit = all_requests + n;
	if (limit != prev_limit) {
		prev_limit = limit;
		prev_found = all_requests;
	}
	req = prev_found;
	for (;;) {
		req = ((req > all_requests) ? req : limit) - 1;
		if (req->dev < 0)
			break;
		if (req == prev_found)
			return NULL;
	}
	prev_found = req;
	req->dev = dev;
	return req;
}

static long ro_bits[MAX_BLKDEV][8];

int is_read_only(int dev) {
	int major, minor;

	major = MAJOR(dev);
	minor = MINOR(dev);
	if (major < 0 || major >= MAX_BLKDEV) return 0;
	return ro_bits[major][minor >> 5] & (1 << (minor & 31));
}

void set_device_ro(int dev, int flag) {
	int major, minor;

	major = MAJOR(dev);
	minor = MINOR(dev);
	if (major < 0 || major >= MAX_BLKDEV) return;
	if (flag) ro_bits[major][minor >> 5] |= 1 << (minor & 31);
	else ro_bits[major][minor >> 5] &= ~(1 << (minor & 31));
}

static void add_request(struct blk_dev_struct * dev, struct request * req) {
	struct request *tmp;
	req->next = NULL;
	cli();
	if (req->bh)
		req->bh->b_dirt = 0;
	if (!(tmp = dev->current_request)) {
		dev->current_request = req;
		(dev->request_fn)();
		sti();
		return;
	}
	for (; tmp->next; tmp = tmp->next) {
		if ((IN_ORDER(tmp, req) ||
			!IN_ORDER(tmp, tmp->next)) &&
			IN_ORDER(req, tmp->next))
			break;
	}
	req->next = tmp->next;
	tmp->next = req;
	// 对于SCSI设备，无条件执行
	if (scsi_major(MAJOR(req->dev)))
		(dev->request_fn)();
	sti();
}

static void make_request(int major, int rw, struct buffer_head *bh) {
	unsigned int sector, count;
	struct request * req;
	int rw_ahead, max_req;

	rw_ahead = (rw == READA || rw == WRITEA);
	if (rw_ahead) {
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
	if (rw != READ && rw != WRITE) {
		printk("Bad block dev command, must be R/W/RA/WA\n");
		return;
	}
	// 多少个512字节
	count = bh->b_size >> 9;
	sector = bh->b_blocknr * count;
	if (blk_size[major])
		if (blk_size[major][MINOR(bh->b_dev)] < (sector + count) >> 1) {
			bh->b_dirt = bh->b_uptodate = 0;
			return;
		}
	lock_buffer(bh);
	// 如果是要写，那bh必须是脏的；如果是要读，那bh必须是未更新的
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		unlock_buffer(bh);
		return;
	}
	max_req = (rw == READ) ? NR_REQUEST : ((NR_REQUEST * 2) / 3);
repeat:
	cli();
	if ((major == HD_MAJOR 
		|| major == SCSI_DISK_MAJOR 
		|| major == SCSI_CDROM_MAJOR) 
		&& (req = blk_dev[major].current_request)) {
        if (major == HD_MAJOR)
			req = req->next;
		while (req) {
			if (req->dev == bh->b_dev &&
				!req->waiting &&
				req->cmd == rw &&
				req->sector + req->nr_sectors == sector &&
				req->nr_sectors < 254) {
					req->bhtail->b_reqnext = bh;
					req->bhtail = bh;
					req->nr_sectors += count;
					bh->b_dirt = 0;
					sti();
					return;
				}

			if (req->dev == bh->b_dev &&
				!req->waiting &&
				req->cmd == rw &&
				req->sector - count == sector &&
				req->nr_sectors < 254) {
					req->nr_sectors += count;
					bh->b_reqnext = req->bh;
					req->buffer = bh->b_data;
					req->current_nr_sectors = count;
					req->sector = sector;
					bh->b_dirt = 0;
					req->bh = bh;
					sti();
					return;
				}
			req = req->next;
		}
	}
	req = get_request(max_req, bh->b_dev);
	if (!req) {
		if (rw_ahead) {
			sti();
			unlock_buffer(bh);
			return;
		}
		sleep_on(&wait_for_request);
		sti();
		goto repeat;
	}
	sti();
	req->cmd = rw;
	req->errors = 0;
	req->sector = sector;
	req->nr_sectors = count;
	req->current_nr_sectors = count;
	req->buffer = bh->b_data;
	req->waiting = NULL;
	req->bh = bh;
	req->bhtail = bh;
	req->next = NULL;
	add_request(major + blk_dev, req);
}

void ll_rw_block(int rw, int nr, struct buffer_head * bh[]) {
	unsigned int major;
	struct request plug;
	int plugged;
	int correct_size;
	struct blk_dev_struct * dev;
	int i;

	while (!*bh) {
		bh++;
		if (--nr <= 0)
			return;
	}

	dev = NULL;
	if ((major = MAJOR(bh[0]->b_dev)) < MAX_BLKDEV)
		dev = blk_dev + major;
	if (!dev || !dev->request_fn) {
		printk("ll_rw_block: Trying to read nonexistent block-device %04lX (%ld)\n",
		       (unsigned long) bh[0]->b_dev, bh[0]->b_blocknr);
		goto sorry;
	}

	correct_size = BLOCK_SIZE;
	if (blksize_size[major]) {
		i = blksize_size[major][MINOR(bh[0]->b_dev)];
		if (i)
			correct_size = i;
	}

	for (i = 0; i < nr; i++) {
		if (bh[i] && bh[i]->b_size != correct_size) {
			printk("ll_rw_block: only %d-char blocks implemented (%lu)\n",
			       correct_size, bh[i]->b_size);
			goto sorry;
		}
	}

	if ((rw == WRITE || rw == WRITEA) && is_read_only(bh[0]->b_dev)) {
		printk("Can't write to read-only device 0x%X\n", bh[0]->b_dev);
		goto sorry;
	}

	plugged = 0;
	cli();
	if (!dev->current_request && nr > 1) {
		dev->current_request = &plug;
		plug.dev = -1;
		plug.next = NULL;
		plugged = 1;
	}
	sti();
	for (i = 0; i < nr; i++) {
		if (bh[i]) {
			bh[i]->b_req = 1;
			make_request(major, rw, bh[i]);
			if (rw == READ || rw == READA)
				kstat.pgpgin++;
			else
				kstat.pgpgout++;
		}
	}
	if (plugged) {
		cli();
		dev->current_request = plug.next;
		(dev->request_fn)();
		sti();
	}
	return;

sorry:
	for (i = 0; i < nr; i++) {
		if (bh[i])
			bh[i]->b_dirt = bh[i]->b_uptodate = 0;
	}
	return;
}

long blk_dev_init(long mem_start, long mem_end) {
    struct request *req;

    req = all_requests + NR_REQUEST;
    while (--req >= all_requests) {
        req->dev = -1;
        req->next = NULL;
    }
    memset(ro_bits, 0, sizeof(ro_bits));
#ifdef CONFIG_BLK_DEV_HD
    mem_start = hd_init(mem_start, mem_end);
#endif

    return mem_start;
}