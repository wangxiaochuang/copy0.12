#include <stdarg.h>

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/system.h>
#include <asm/io.h>

// 编译时的连接程序ld会生成，表示内核代码的末端
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

static void sync_buffers(int dev) {
    int i;
    struct buffer_head *bh;

    bh = free_list;
    for (i = 0; i < NR_BUFFERS; i++, bh = bh->b_next_free) {
        wait_on_buffer(bh);
        if (bh->b_dirt)
            ll_rw_block(WRITE, bh);
    }
}

int sys_sync(void) {
    sync_inodes();
    sync_buffers(0);
    return 0;
}

int sync_dev(int dev) {
    sync_buffers(dev);
	sync_inodes();
	sync_buffers(dev);
	return 0;
}

static inline void invalidate_buffers(int dev) {
    int i;
    struct buffer_head *bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev) {
            bh->b_uptodate = bh->b_dirt = 0;
        }
    }
}

void check_disk_change(int dev) {
    int i;
    // 只处理软盘
    if (MAJOR(dev) != 2) {
        return;
    }
    if (!floppy_change(dev & 0x03)) {
        return;
    }
    for (i = 0; i < NR_SUPER; i++) {
        if (super_block[i].s_dev == dev) {
            put_super(super_block[i].s_dev);
        }
    }
    invalidate_inodes(dev);
	invalidate_buffers(dev);
}

#define _hashfn(dev, block) (((unsigned)(dev ^ block)) % NR_HASH)
#define hash(dev, block) 	hash_table[_hashfn(dev, block)]

static inline void remove_from_hash_queue(struct buffer_head *bh) {
    if (bh->b_next)
        bh->b_next->b_prev = bh->b_prev;
    if (bh->b_prev)
        bh->b_prev->b_next = bh->b_next;
    // hash头
    if (hash(bh->b_dev, bh->b_blocknr) == bh)
        hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
    bh->b_next = bh->b_prev = NULL;
}

static inline void remove_from_free_list(struct buffer_head *bh) {
    if (!(bh->b_prev_free) || !(bh->b_next_free))
        panic("Free block list corrupted");
    bh->b_prev_free->b_next_free = bh->b_next_free;
    bh->b_next_free->b_prev_free = bh->b_prev_free;
    if (free_list == bh)
        free_list = bh->b_next_free;
    bh->b_next_free = bh->b_prev_free = NULL;
}

static inline void remove_from_queues(struct buffer_head * bh) {
    remove_from_hash_queue(bh);
    remove_from_free_list(bh);
}

static inline void put_first_free(struct buffer_head *bh) {
    if (!bh || (bh == free_list))
        return;
    remove_from_free_list(bh);
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
    free_list = bh;
}

static inline void put_last_free(struct buffer_head *bh) {
    if (!bh)
        return;
    if (bh == free_list) {
        free_list = bh->b_next_free;
        return;
    }
    remove_from_free_list(bh);
    // 将bh放到free_list的末尾
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
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

/**
 * [dev^block % 307]->b_next
 * [dev^block % 307]->b_next
 * [dev^block % 307]->b_next
 * ...
 **/
static struct buffer_head * find_buffer(int dev, int block) {		
	struct buffer_head * tmp;

	for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next)
		if (tmp->b_dev == dev && tmp->b_blocknr == block)
			return tmp;
	return NULL;
}

struct buffer_head * get_hash_table(int dev, int block)
{
	struct buffer_head * bh;

	for (;;) {
        // 在hash表中找到了
		if (!(bh = find_buffer(dev, block))) {
			return NULL;
		}
		bh->b_count++;
		wait_on_buffer(bh);
        // 睡眠期间可能变化了，所以要再判断一下
		if (bh->b_dev == dev && bh->b_blocknr == block) {
            // 放到free_list的末尾
            put_last_free(bh);
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
    struct buffer_head *bh, *tmp;
    int buffers;
repeat:
    if ((bh = get_hash_table(dev, block)))
		return bh;
    buffers = NR_BUFFERS;
    tmp = free_list;
    do {
        tmp = tmp->b_next_free;
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
        if (tmp->b_dirt)
            ll_rw_block(WRITEA, tmp);
    } while (buffers--);
    if (!bh) {
        // 没找到，我就睡了，有空闲的bh了，请叫醒我
        sleep_on(&buffer_wait);
        goto repeat;
    }
    wait_on_buffer(bh);
    // bh解锁后发现还有被占用，就继续找
    if (bh->b_count)
        goto repeat;
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

    // end - 4M
    // 从end到4M，开始是存放 struct buffer_head 列表
    while ((b -= BLOCK_SIZE) >= ((void *) (h + 1))) {
        // buffer_head最多到0xA0000
        if (((unsigned long) (h + 1)) > 0xA0000) {
            printk("buffer-list doesn't fit in low meg - contact Linus\n");
            break;
        }
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