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

int * blk_size[MAX_BLKDEV] = { NULL, NULL, };

int * blksize_size[MAX_BLKDEV] = { NULL, NULL, };

static long ro_bits[MAX_BLKDEV][8];

void ll_rw_block(int rw, int nr, struct buffer_head * bh[]) {
	
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