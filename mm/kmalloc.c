#include <linux/mm.h>
#include <asm/system.h>
#include <linux/delay.h>

#define GFP_LEVEL_MASK 0

#define MAX_KMALLOC_K 4

#define MAX_GET_FREE_PAGE_TRIES 4

#define MF_USED 0xffaa0055
#define MF_FREE 0x0055ffaa

struct block_header {
	unsigned long bh_flags;
	union {
		unsigned long ubh_length;
		struct block_header *fbh_next;
	} vp;
};

#define bh_length vp.ubh_length
#define bh_next   vp.fbh_next
#define BH(p) ((struct block_header *)(p))

/**
 * 每一个page前面都有这个描述结构
 **/
struct page_descriptor {
	struct page_descriptor *next;
	struct block_header *firstfree;
	int order;
	int nfree;
};

#define PAGE_DESC(p) ((struct page_descriptor *)(((unsigned long)(p)) & PAGE_MASK))

/**
 * 描述了可被分配的内存类型块，每一种类型都有其自己的列表
 * 
 **/
struct size_descriptor {
	struct page_descriptor *firstfree;
	int size;                           // 每一个的大小
	int nblocks;                        // 有多少个

	int nmallocs;                       // 分配了多少个
	int nfrees;                         // 多少个空闲的
	int nbytesmalloced;                 // 已经分配的字节数
	int npages;                         // 多少个页
};

struct size_descriptor sizes[] = { 
	{ NULL,  32,127, 0,0,0,0 },
	{ NULL,  64, 63, 0,0,0,0 },
	{ NULL, 128, 31, 0,0,0,0 },
	{ NULL, 252, 16, 0,0,0,0 },
	{ NULL, 508,  8, 0,0,0,0 },
	{ NULL,1020,  4, 0,0,0,0 },
	{ NULL,2040,  2, 0,0,0,0 },
	{ NULL,4080,  1, 0,0,0,0 },
	{ NULL,   0,  0, 0,0,0,0 }
};

#define NBLOCKS(order)          (sizes[order].nblocks)
#define BLOCKSIZE(order)        (sizes[order].size)

// 只是检查初始化设置的这些是否超出一页大小
long kmalloc_init(long start_mem, long end_mem) {
    int order;

    for (order = 0; BLOCKSIZE(order); order++) {
        if ((NBLOCKS(order) * BLOCKSIZE(order) + sizeof(struct page_descriptor)) > PAGE_SIZE) {
            panic("This only happens if someone messes with kmalloc");
        }
    }
    return start_mem;
}

int get_order(int size) {
	int order;
	size += sizeof(struct block_header);
	for (order = 0; BLOCKSIZE(order); order++)
		if (size <=  BLOCKSIZE(order))
			return order;
	return -1;
}

void *kmalloc(size_t size, int priority) {
	unsigned long flags;
	int order,tries,i,sz;
	struct block_header *p;
	struct page_descriptor *page;
	extern unsigned long intr_count;

	if (intr_count && priority != GFP_ATOMIC) {
		printk("kmalloc called nonatomically from interrupt %08lx\n",
			((unsigned long *)&size)[-1]);
		priority = GFP_ATOMIC;
	}

	if (size > MAX_KMALLOC_K * 1024) {
		printk ("kmalloc: I refuse to allocate %d bytes (for now max = %d).\n",
                size, MAX_KMALLOC_K*1024);
		return (NULL);
	}
	order = get_order(size);
	if (order < 0) {
		printk ("kmalloc of too large a block (%d bytes).\n",size);
		return (NULL);
	}
	save_flags(flags);
	tries = MAX_GET_FREE_PAGE_TRIES;
	while (tries--) {
		cli();
		if ((page = sizes[order].firstfree) && (p = page->firstfree)) {
			if (p->bh_flags == MF_FREE) {
				page->firstfree = p->bh_next;
				page->nfree--;
				// 这一页已经没有空闲块了，就找到下一页，当前还是找到了的
				if (!page->nfree) {
					sizes[order].firstfree = page->next;
					page->next = NULL;
				}
				restore_flags(flags);

				sizes[order].nmallocs++;
				sizes[order].nbytesmalloced += size;
				p->bh_flags = MF_USED;
				p->bh_length = size;
				/**
				 * 每一个block_header前面部分是其元数据，紧接着的bh_length是数据区域
				 * 这里p+1其实就是数据区域的地址
				 **/
				return p + 1;
			}
			printk ("Problem: block on freelist at %08lx isn't free.\n",(long)p);
			return (NULL);
		}
		restore_flags(flags);
		sz = BLOCKSIZE(order);
		page = (struct page_descriptor *) __get_free_page(priority & GFP_LEVEL_MASK);
		if (!page) {
			printk ("Couldn't get a free page.....\n");
			return NULL;
		}
		sizes[order].npages++;
		/**
		 * page + 1就预留了一个page_descriptor的位置
		 * 后面继续作为block_header使用
		 * 初始化的时候已经判断了大小，一定不会超过一页
		 **/
		for (i = NBLOCKS(order), p = BH(page + 1); i > 1; i--, p = p->bh_next) {
			p->bh_flags = MF_FREE;
			p->bh_next = BH(((long) p) + sz);
		}
		// the last block
		p->bh_flags = MF_FREE;
		p->bh_next = NULL;

		// 这一页内存是作为order使用
		page->order = order;
		page->nfree = NBLOCKS(order);
		// 这一页内存中的第一个空闲的block_header
		page->firstfree = BH(page + 1);
		cli();
		page->next = sizes[order].firstfree;
		// 挂在sizes上
		sizes[order].firstfree = page;
		restore_flags(flags);
	}
	printk ("Hey. This is very funny. I tried %d times to allocate a whole\n"
        "new page for an object only %d bytes long, but some other process\n"
        "beat me to actually allocating it. Also note that this 'error'\n"
        "message is soooo very long to catch your attention. I'd appreciate\n"
        "it if you'd be so kind as to report what conditions caused this to\n"
        "the author of this kmalloc: wolff@dutecai.et.tudelft.nl.\n"
        "(Executive summary: This can't happen)\n", 
                MAX_GET_FREE_PAGE_TRIES,
                size);
	return NULL;
}

void kfree_s (void *ptr, int size) {
	unsigned long flags;
	int order;
	register struct block_header *p = ((struct block_header *) ptr) - 1;
	struct page_descriptor *page, *pg2;

	page = PAGE_DESC(p);
	order = page->order;
	if ((order < 0) ||
		(order > sizeof(sizes) / sizeof(sizes[0])) ||
		(((long) (page->next)) & ~PAGE_MASK) ||
		(p->bh_flags != MF_USED)) {
			printk ("kfree of non-kmalloced memory: %p, next= %p, order=%d\n",
                p, page->next, page->order);
			return;
		}
	if (size && size != p->bh_length) {
		printk ("Trying to free pointer at %p with wrong size: %d instead of %lu.\n",
				p,size,p->bh_length);
		return;
	}
	size = p->bh_length;
	p->bh_flags = MF_FREE;

	save_flags(flags);
	cli();
	p->bh_next = page->firstfree;
	page->firstfree = p;
	page->nfree++;
	if (page->nfree == 1) {
		if (page->next) {
			printk ("Page %p already on freelist dazed and confused....\n", page);
		} else {
			page->next = sizes[order].firstfree;
			sizes[order].firstfree = page;
		}
	}
	if (page->nfree == NBLOCKS(page->order)) {
		if (sizes[order].firstfree == page) {
			sizes[order].firstfree = page->next;
		} else {
			for (pg2 = sizes[order].firstfree; 
				(pg2 != NULL) && (pg2->next != page); 
					pg2 = pg2->next)
					;
			if (pg2 != NULL)
				pg2->next = page->next;
			else
				printk ("Ooops. page %p doesn't show on freelist.\n", page);
		}
		free_page((long) page);
	}
	restore_flags(flags);
	sizes[order].nfrees++;
	sizes[order].nbytesmalloced -= size;
}