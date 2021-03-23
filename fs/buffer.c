#include <stdarg.h>
 
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/io.h>

static int grow_buffers(int pri, int size);

static struct buffer_head * hash_table[NR_HASH];
static struct buffer_head * free_list = NULL;
static struct buffer_head * unused_list = NULL;
static struct wait_queue * buffer_wait = NULL;

int nr_buffers = 0;
int buffermem = 0;
int nr_buffer_heads = 0;
static int min_free_pages = 20;

void __wait_on_buffer(struct buffer_head * bh) {
    struct wait_queue wait = { current, NULL };

    bh->b_count++;
    add_wait_queue(&bh->b_wait, &wait);
repeat:
    current->state = TASK_UNINTERRUPTIBLE;
    if (bh->b_lock) {
        schedule();
        goto repeat;
    }
    remove_wait_queue(&bh->b_wait, &wait);
    bh->b_count--;
    current->state = TASK_RUNNING;
}

static int sync_buffers(dev_t dev, int wait) {
    int i, retry, pass = 0, err = 0;
    struct buffer_head *bh;

repeat:
    retry = 0;
    bh = free_list;
    for (i = nr_buffers * 2; i-- > 0; bh = bh->b_next_free) {
        if (dev && bh->b_dev != dev)
            continue;
        if (bh->b_lock) {
            if (!wait || !pass) {
                retry = 1;
                continue;
            }
            wait_on_buffer(bh);
        }
        if (wait && bh->b_req && !bh->b_lock && !bh->b_dirt && !bh->b_uptodate) {
            err = 1;
            continue;
        }
        /* Don't write clean buffers.  Don't write ANY buffers
		   on the third pass. */
        if (!bh->b_dirt || pass >= 2)
            continue;
        bh->b_count++;
        ll_rw_block(WRITE, 1, &bh);
        bh->b_count--;
        retry = 1;
    }
    if (wait && retry && ++pass <= 2)
        goto repeat;
    return err;
}

#define _hashfn(dev, block) (((unsigned)(dev^block)) % NR_HASH)
#define hash(dev, block) hash_table[_hashfn(dev, block)]

static inline void remove_from_hash_queue(struct buffer_head * bh) {
    if (bh->b_next)
        bh->b_next->b_prev = bh->b_prev;
    if (bh->b_prev)
        bh->b_prev->b_next = bh->b_next;
    if (hash(bh->b_dev, bh->b_blocknr) == bh)
        hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
    bh->b_next = bh->b_prev = NULL;
}

static inline void remove_from_free_list(struct buffer_head * bh) {
    if (!(bh->b_prev_free) || !(bh->b_next_free))
        panic("VFS: Free block list corrupted");
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

/**
 * 插入到free_list的最前面
 **/
static inline void put_first_free(struct buffer_head * bh) {
    if (!bh || (bh == free_list))
        return;
    remove_from_free_list(bh);
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
    free_list = bh;
}

/**
 * 插入到free_list的最后面
 **/
static inline void put_last_free(struct buffer_head * bh) {
    if (!bh)
        return;
    if (bh == free_list) {
        free_list = bh->b_next_free;
        return;
    }
    remove_from_free_list(bh);
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
}

static inline void insert_into_queues(struct buffer_head * bh) {
    // 放到free_list的尾部
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;

    bh->b_prev = NULL;
    bh->b_next = NULL;
    if (!bh->b_dev)
        return;
    bh->b_next = hash(bh->b_dev, bh->b_blocknr);
    hash(bh->b_dev, bh->b_blocknr) = bh;
    if (bh->b_next)
        bh->b_next->b_prev = bh;
}

static struct buffer_head * find_buffer(dev_t dev, int block, int size) {
    struct buffer_head *tmp;

    for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next)
        if (tmp->b_dev == dev && tmp->b_blocknr == block) {
            if (tmp->b_size == size)
                return tmp;
            else {
                printk("VFS: Wrong blocksize on device %d/%d\n",
							MAJOR(dev), MINOR(dev));
				return NULL;
            }
        }
    return NULL;
}

struct buffer_head * get_hash_table(dev_t dev, int block, int size) {
    struct buffer_head *bh;

    for (;;) {
        if (!(bh = find_buffer(dev, block, size)))
            return NULL;
        bh->b_count++;
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_blocknr == block && bh->b_size == size)
            return bh;
        bh->b_count--;
    }
}

#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)
struct buffer_head * getblk(dev_t dev, int block, int size) {
    struct buffer_head *bh, *tmp;
    int buffers;
    static int grow_size = 0;

repeat:
    bh = get_hash_table(dev, block, size);
    if (bh) {
        if (bh->b_uptodate && !bh->b_dirt)
            put_last_free(bh);
        return bh;
    }
    grow_size -= size;
    if (nr_free_pages > min_free_pages && grow_size <= 0) {
        if (grow_buffers(GFP_BUFFER, size))
            grow_size = PAGE_SIZE;
    }
    buffers = nr_buffers;
    bh = NULL;

    for (tmp = free_list; buffers-- > 0; tmp = tmp->b_next_free) {
        if (tmp->b_count || tmp->b_size != size)
            continue;
        if (mem_map[MAP_NR((unsigned long) tmp->b_data)] != 1)
            continue;
        if (!bh || BADNESS(tmp) < BADNESS(bh)) {
            bh = tmp;
            if (!BADNESS(tmp))
                break;
        }
    }
    if (!bh) {
        if (nr_free_pages > 5)
            if (grow_buffers(GFP_BUFFER, size))
                goto repeat;
        if (!grow_buffers(GFP_ATOMIC, size))
            sleep_on(&buffer_wait);
        goto repeat;
    }
    wait_on_buffer(bh);
    if (bh->b_count || bh->b_size != size)
        goto repeat;
    if (bh->b_dirt) {
        sync_buffers(0, 0);
        goto repeat;
    }
    if (find_buffer(dev, block, size))
        goto repeat;
    bh->b_count = 1;
    bh->b_dirt = 0;
    bh->b_uptodate = 0;
    bh->b_req = 0;
    remove_from_queues(bh);
    bh->b_dev = dev;
    bh->b_blocknr = block;
    insert_into_queues(bh);
    return bh;
}

void brelse(struct buffer_head * buf) {
    if (!buf)
        return;
    wait_on_buffer(buf);
    if (buf->b_count) {
        if (--buf->b_count)
            return;
        wake_up(&buffer_wait);
        return;
    }
    printk("VFS: brelse: Trying to free free buffer\n");
}

struct buffer_head * bread(dev_t dev, int block, int size) {
    struct buffer_head *bh;

    if (!(bh = getblk(dev, block, size))) {
        printk("VFS: bread: READ error on device %d/%d\n",
						MAJOR(dev), MINOR(dev));
		return NULL;
    }
    if (bh->b_uptodate)
        return bh;
    ll_rw_block(READ, 1, &bh);
    wait_on_buffer(bh);
    if (bh->b_uptodate)
        return bh;
    brelse(bh);
    return NULL;
}

static void put_unused_buffer_head(struct buffer_head * bh) {
    struct wait_queue *wait;
    wait = ((volatile struct buffer_head *) bh)->b_wait;
    memset((void *) bh, 0, sizeof(*bh));
    ((volatile struct buffer_head *) bh)->b_wait = wait;
    bh->b_next_free = unused_list;
    unused_list = bh; 
}

/**
 * 申请一页内存来存放buffer_head，通过b_next_free连接
 * unused_list指向这些空闲备用的缓冲区头，并没有数据
 **/
static void get_more_buffer_heads(void) {
    int i;
    struct buffer_head *bh;

    if (unused_list)
        return;
    if (!(bh = (struct buffer_head *) get_free_page(GFP_BUFFER)))
        return;
    
    for (nr_buffer_heads += i = PAGE_SIZE / sizeof *bh; i > 0; i--) {
        bh->b_next_free = unused_list;
        unused_list = bh++;
    }
}

// 获取一个缓冲区头
static struct buffer_head * get_unused_buffer_head(void) {
    struct buffer_head *bh;

    get_more_buffer_heads();
    if (!unused_list)
        return NULL;
    bh = unused_list;
    unused_list = bh->b_next_free;
    bh->b_next_free = NULL;
    bh->b_data = NULL;
    bh->b_size = 0;
    bh->b_req = 0;
    return bh;

}

static struct buffer_head * create_buffers(unsigned long page, unsigned long size) {
    struct buffer_head *bh, *head;
    unsigned long offset;

    // head指向当前page的第一个缓冲区，同一个page中的缓冲区通过b_this_page连接起来
    head = NULL;
    offset = PAGE_SIZE;
    while ((offset -= size) < PAGE_SIZE) {
        bh = get_unused_buffer_head();
        if (!bh)
            goto no_grow;
        bh->b_this_page = head;
        head = bh;
        // 从后往前
        bh->b_data = (char *) (page + offset);
        bh->b_size = size;
    }
    // 通过b_this_page可以找到当前page的所有buffer_head
    return head;
    
no_grow:
    bh = head;
    while (bh) {
        head = bh;
        bh = bh->b_this_page;
        put_unused_buffer_head(head);
    }
    return NULL;
}

static int grow_buffers(int pri, int size) {
    unsigned long page;
    struct buffer_head *bh, *tmp;

    if ((size & 511) || (size > PAGE_SIZE)) {
        printk("VFS: grow_buffers: size = %d\n",size);
        return 0;
    }
    if (!(page = __get_free_page(pri)))
        return 0;
    // 这一页内存将分配到多个buffer_head里，每个buffer_head可以表示size的空间
    // 返回这一页内存创建的多个buffer_head的头，通过b_this_page连接
    bh = create_buffers(page, size);
    if (!bh) {
        free_page(page);
        return 0;
    }
    tmp = bh;
    while (1) {
        // 将这个bh通过插入到free_list的前面
        if (free_list) {
            tmp->b_next_free = free_list;
            tmp->b_prev_free = free_list->b_prev_free;
            free_list->b_prev_free->b_next_free = tmp;
            free_list->b_prev_free = tmp;
        } else {
            // free_list为空，这是第一个
            tmp->b_prev_free = tmp;
            tmp->b_next_free = tmp;
        }
        free_list = tmp;
        ++nr_buffers;
        // 继续处理当前page的下一个bufer_head
        if (tmp->b_this_page)
            tmp = tmp->b_this_page;
        else
            // 当前page的所有bh都放到free_list了
            break;
    }
    // 当前page的所有缓冲区通过b_this_page指针连接
    tmp->b_this_page = bh;
    buffermem += PAGE_SIZE;
    return 1;
}

int shrink_buffers(unsigned int priority) {
    return 0;
}

void buffer_init(void) {
    int i;
    if (high_memory >= 4 * 1024 * 1024)
        min_free_pages = 200;
    else
        min_free_pages = 20;
    
    for (i = 0; i < NR_HASH; i++)
        hash_table[i] = NULL;
    free_list = 0;
    grow_buffers(GFP_KERNEL, BLOCK_SIZE);
    if (!free_list)
        panic("VFS: Unable to initialize buffer free list!");
    return;
}