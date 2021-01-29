#include <stdarg.h>

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/system.h>
#include <asm/io.h>

// @todo
extern int end;

// struct buffer_head *start_buffer = (struct buffer_head *) (200 * 1024);
struct buffer_head *start_buffer = (struct buffer_head *) & end;

struct buffer_head *hash_table[NR_HASH];

static struct buffer_head *free_list;

static struct task_struct *buffer_wait = NULL;

int NR_BUFFERS = 0;

static inline void wait_on_buffer(struct buffer_head * bh) {
	cli();
	while (bh->b_lock) {	/* 如果已被上锁则进程进入睡眠，等待其解锁 */
		sleep_on(&bh->b_wait);
	}
	sti();
}

int sync_dev(int dev) {
    int i;
    struct buffer_head *bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt) {
            ll_rw_block(WRITE, bh);
        }
    }
    sync_inodes();
    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt) {
            ll_rw_block(WRITE, bh);
        }
    }
    return 0;
}

#define _hashfn(dev, block) (((unsigned)(dev ^ block)) % NR_HASH)
#define hash(dev, block) 	hash_table[_hashfn(dev, block)]

static inline void remove_from_queues(struct buffer_head * bh) {
    // 从hash队列中移除
	if (bh->b_next) {
		bh->b_next->b_prev = bh->b_prev;
	}
	if (bh->b_prev) {
		bh->b_prev->b_next = bh->b_next;
	}
    // 要移除的是hash表的第一块，则让hash表的该位置指向下一个
	if (hash(bh->b_dev, bh->b_blocknr) == bh) {
		hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
	}
    // 从空闲缓冲块表中移除缓冲块
	if (!(bh->b_prev_free) || !(bh->b_next_free)) {
		panic("Free block list corrupted");
	}
	bh->b_prev_free->b_next_free = bh->b_next_free;
	bh->b_next_free->b_prev_free = bh->b_prev_free;
	// 如果空闲链表头指向本缓冲区，则让其指向下一缓冲区
	if (free_list == bh) {
		free_list = bh->b_next_free;
	}
}

static inline void insert_into_queues(struct buffer_head * bh) {
	// 放在空闲链表末尾处
	bh->b_next_free = free_list;
	bh->b_prev_free = free_list->b_prev_free;
	free_list->b_prev_free->b_next_free = bh;
	free_list->b_prev_free = bh;
	// 如果该缓冲块对应一个设备,则将其插入新hash队列中
	bh->b_prev = NULL;
	bh->b_next = NULL;
	if (!bh->b_dev) {
		return;
	}
	bh->b_next = hash(bh->b_dev,bh->b_blocknr);
	hash(bh->b_dev,bh->b_blocknr) = bh;
	if (bh->b_next) {	
		bh->b_next->b_prev = bh;
	}
}

static struct buffer_head * find_buffer(int dev, int block)
{		
	struct buffer_head * tmp;

	for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next) {
		if (tmp->b_dev == dev && tmp->b_blocknr == block) {
			return tmp;
		}
	}
	return NULL;
}

struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (;;) {
		if (!(bh = find_buffer(dev, block))) {
			return NULL;
		}
		bh->b_count ++;
		wait_on_buffer(bh);
		if (bh->b_dev == dev && bh->b_blocknr == block) {
			return bh;
		}
		bh->b_count--;
	}
}

#define BADNESS(bh) (((bh)->b_dirt << 1) + (bh)->b_lock)

/**
 * 先到hash表中通过dev和block找一个bh，不存在就到空闲列表中找一个，
 * 填充好后放到hash表
 **/
struct buffer_head *getblk(int dev, int block) {
    struct buffer_head *tmp, *bh = NULL;
repeat:
    if ((bh = get_hash_table(dev, block))) {
		return bh;
	}
    tmp = free_list;
    do {
        if (tmp->b_count) {
            continue;
        }
        // 如果两个bh，一个只是修改了，一个只是锁定了，那就取锁定的那个
        if (!bh || BADNESS(tmp) < BADNESS(bh)) {
            bh = tmp;
            // 既没修改也没锁定
            if (!BADNESS(tmp)) {
                break;
            }
        }
    } while ((tmp = tmp->b_next_free) != free_list);
    if (!bh) {
        // 没找到，我就睡了，有空闲的bh了，请叫醒我
        sleep_on(&buffer_wait);
        goto repeat;
    }
    wait_on_buffer(bh);
    // bh解锁后发现还有被占用，就继续找
    if (bh->b_count) {
        goto repeat;
    }
    // 找到的这个有写入，就同步
    while (bh->b_dirt) {
        sync_dev(bh->b_dev);
        wait_on_buffer(bh);
        if (bh->b_count) {
            goto repeat;
        }
    }
    // 睡眠期间可能该缓冲块已经加入到高速缓冲中
    if (find_buffer(dev, block)) {
        goto repeat;
    }
    bh->b_count = 1;
    bh->b_dirt = 0;
    bh->b_uptodate = 0;

    remove_from_queues(bh);
    bh->b_dev = dev;
    bh->b_blocknr = block;
    insert_into_queues(bh);

    return bh;
}

void brelse(struct buffer_head * buf) {
	if (!buf) {
		return;
	}
	wait_on_buffer(buf);
	if (!(buf->b_count--)) {
		panic("Trying to free free buffer");
	}
	wake_up(&buffer_wait);
}

// 通过块号将数据映射到高速缓冲区
struct buffer_head *bread(int dev, int block) {
    struct buffer_head *bh;

    if (!(bh = getblk(dev, block))) {
        panic("bread: getblk returned NULL\n");
    }
    if (bh->b_uptodate) {
        return bh;
    }
    ll_rw_block(READ, bh);
    wait_on_buffer(bh);
    if (bh->b_uptodate) {
        return bh;
    }
    brelse(bh);
    return NULL;
}

#define COPYBLK(from, to) 								\
__asm__(												\
	"cld\n\t" 											\
	"rep\n\t" 											\
	"movsl\n\t" 										\
	:													\
	:"c" (BLOCK_SIZE/4),"S" (from),"D" (to) 			\
	)

void bread_page(unsigned long address, int dev, int b[4]) {
    struct buffer_head *bh[4];
    int i;

    for (i = 0; i < 4; i++) {
        if (b[i]) {
            if ((bh[i] = getblk(dev, b[i]))) {
                // 如果缓冲区数据还未更新，就产生读请求
                if (!bh[i]->b_uptodate) {
                    ll_rw_block(READ, bh[i]);
                }
            }
        } else {
            bh[i] = NULL;
        }
    }
    for (i = 0; i < 4; i++, address += BLOCK_SIZE) {
        if (bh[i]) {
            wait_on_buffer(bh[i]);
            if (bh[i]->b_uptodate) {
                COPYBLK((unsigned long) bh[i]->b_data, address);
            }
            brelse(bh[i]);
        }
    }
}

struct buffer_head * breada(int dev, int first, ...) {
    va_list args;
    struct buffer_head *bh, *tmp;

    va_start(args, first);
    if (!(bh = getblk(dev, first))) {
        panic("bread: getblk returned NULL\n");
    }
    // 找到了
    if (!bh->b_uptodate) {
        ll_rw_block(READ, bh);
    }
    while ((first = va_arg(args, int)) >= 0) {
        tmp = getblk(dev, first);
        if (tmp) {
            if (!tmp->b_uptodate) {
                ll_rw_block(READA, tmp);
            }
            tmp->b_count--;
        }
    }
    va_end(args);
    wait_on_buffer(bh);
    if (bh->b_uptodate) {
        return bh;
    }
    brelse(bh);
    return NULL;
}

void buffer_init(long buffer_end) {
    struct buffer_head *h = start_buffer;
    void *b;
    int i;

    if (buffer_end == 1<<20)
        b = (void *) (640 * 1024);
    else
        b = (void *) buffer_end;

    while ((b -= BLOCK_SIZE) >= ((void *) (h + 1))) {
		h->b_dev = 0;
		h->b_dirt = 0;
		h->b_count = 0;
		h->b_lock = 0;
		h->b_uptodate = 0;
		h->b_wait = NULL;
        // 解决hash冲突的链
		h->b_next = NULL;
		h->b_prev = NULL;

		h->b_data = (char *) b;
        // 空闲双向链表
        h->b_prev_free = h - 1;
        h->b_next_free = h + 1;
        h++;
        NR_BUFFERS++;
        if (b == (void *) 0x100000)
            b = (void *) 0xA0000;
    }
    h--;
    // 空闲链表头
    free_list = start_buffer;
    // 形成一个环
    free_list->b_prev_free = h;
    h->b_next_free = free_list;
    for (i = 0; i < NR_HASH; i++) {
        hash_table[i] = NULL;
    }
}