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

}

#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)
struct buffer_head * getblk(dev_t dev, int block, int size) {
    return NULL;
}

void brelse(struct buffer_head * buf) {
    
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
    return 0;
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