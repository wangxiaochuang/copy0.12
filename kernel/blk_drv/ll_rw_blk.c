#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include "blk.h"

struct request request[NR_REQUEST];

struct task_struct *wait_for_request = NULL;

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL }
};

int * blk_size[NR_BLK_DEV] = { NULL, NULL, };

static inline void lock_buffer(struct buffer_head * bh) {
    cli();
    while (bh->b_lock) {
        sleep_on(&bh->b_wait);
    }
    bh->b_lock = 1;
    sti();
}

static inline void unlock_buffer(struct buffer_head * bh) {
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);
}

static void add_request(struct blk_dev_struct * dev, struct request * req) {
    struct request * tmp;

    req->next = NULL;
    cli();
    if (req->bh)
        req->bh->b_dirt = 0;
    if (!(tmp = dev->current_request)) {
        dev->current_request = req;
        // 既然队列没有请求存在，就直接处理，毕竟当前进程就是为了读数据
        (dev->request_fn)();
        sti();
        return;
    }
    for (; tmp->next; tmp = tmp->next) {
        if (!req->bh) {
            if (tmp->next->bh)
                break;
            else
                continue;
        }
        if ((IN_ORDER(tmp, req) ||
            !IN_ORDER(tmp, tmp->next)) && 
            IN_ORDER(req, tmp->next))
			break;
    }
    req->next = tmp->next;
    tmp->next = req;
    sti();
}

static void make_request(int major, int rw, struct buffer_head * bh) {
    struct request *req;
    int rw_ahead;

    if ((rw_ahead = (rw == READA || rw == WRITEA))) {
        if (bh->b_lock)
            return;
        if (rw == READA)
            rw = READ;
        else
            rw = WRITE;
    }
    if (rw != READ && rw != WRITE)
        panic("Bad block dev command, must be R/W/RA/WA\n");
    lock_buffer(bh);
	// 如果是WRITE操作并且缓冲块未修改，或是READ操作并且缓冲块已更新，则直接返回缓冲区块。
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		unlock_buffer(bh);
		return;
	}
repeat:
    if (rw == READ)
		req = request + NR_REQUEST;
	else
		req = request + ((NR_REQUEST * 2) / 3);
    cli();
    while (--req >= request)
        if (req->dev < 0)
            goto found;
    // 搜索完了都没找到空闲项
    if (rw_ahead) {
        sti();
        unlock_buffer(bh);
        return;
    }
    sleep_on(&wait_for_request);
    sti();
    goto repeat;

found:
    sti();
    req->dev = bh->b_dev;
    req->cmd = rw;
    req->errors = 0;
    req->sector = bh->b_blocknr << 1;       // 起始扇区，1块等于2扇区，底层请求函数时按扇区计数的
    req->nr_sectors = 2;                    // 要求每次读两块
    req->buffer = bh->b_data;
    req->waiting = NULL;
    req->bh = bh;
    req->next = NULL;
    add_request(blk_dev + major, req);
}

void ll_rw_page(int rw, int dev, int page, char *buffer) {
    struct request *req;
    unsigned int major = MAJOR(dev);

    if (major >= NR_BLK_DEV || !(blk_dev[major].request_fn)) {
        printk("Trying to read nonexistent block-device\n\r");
		return;
    }
    if (rw != READ && rw != WRITE)
        panic("Bad block dev command, must be R/W");
    
repeat:
    req = request + NR_REQUEST;
    while (--req >= request)
        if (req->dev < 0)
            break;
    if (req < request) {
        sleep_on(&wait_for_request);
        goto repeat;
    }
    req->dev = dev;
    req->cmd = rw;
    req->errors = 0;
    req->sector = page << 3;    // 起始读写扇区
    req->nr_sectors = 8;        // 8个扇区，4k
    req->buffer = buffer;
    req->waiting = current;
    req->bh = NULL;             // 无缓冲块头指针，即不需要高速缓冲
    req->next = NULL;
    current->state = TASK_UNINTERRUPTIBLE;
    add_request(blk_dev + major, req);
    schedule();
}

void ll_rw_block(int rw, struct buffer_head *bh) {
    unsigned int major;
    if ((major = MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
	        !(blk_dev[major].request_fn)) {
        printk("ll_rw_block: Trying to read nonexistent block-device\n\r");
        return;
    }
    make_request(major, rw, bh);
}

void blk_dev_init(void) {
    int i;
    for (i = 0; i < NR_REQUEST; i++) {
        request[i].dev = -1;
        request[i].next = NULL;
    }
}

void ll_rw_swap_file(int rw, int dev, unsigned int *b, int nb, *buf) {
    int i;
    struct request *req;
    unsigned int major = MAJOR(dev);

    if (major >= NR_BLK_DEV || !(blk_dev[major].request_fn)) {
        printk("ll_rw_swap_file: trying to swap nonexistent block-device\n\r");
		return;
    }
    if (rw != READ && rw != WRITE) {
        printk("ll_rw_swap: bad block dev command, must be R/W");
		return;
    }
    for (i = 0; i < nb; i++, buf += BLOCK_SIZE) {
repeat:
        req = request + NR_REQUEST;
        while (--req >= request)
            if (req->dev < 0)
                break;
        if (req < request) {
            sleep_on(&wait_for_request);
            goto repeat;
        }
        req->dev = dev;
        req->cmd = rw;
        req->errors = 0;
        req->sector = b[i] << 1;
        req->nr_sectors = 2;
        req->buffer = buf;
        req->waiting = current;
        req->bh = NULL;
        req->next = NULL;
        current->state = TASK_UNINTERRUPTIBLE;
        add_request(major + blk_dev, req);
        schedule();
    }
}