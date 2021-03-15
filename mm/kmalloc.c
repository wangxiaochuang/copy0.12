#include <linux/mm.h>
#include <asm/system.h>

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