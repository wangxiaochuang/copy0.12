#include <asm/system.h>
#include <linux/config.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/malloc.h>
#include <asm/segment.h>

struct vm_struct {
	unsigned long flags;
	void * addr;
	unsigned long size;
	struct vm_struct * next;
};

static struct vm_struct * vmlist = NULL;

#define VMALLOC_OFFSET	(8*1024*1024)

static inline void set_pgdir(unsigned long dindex, unsigned long value) {
    struct task_struct *p;

    p = &init_task;
    // 因为是内核态，所有进程共享，所以每一个进程都要设置
    do {
        ((unsigned long *) p->tss.cr3)[dindex] = value;
        p = p->next_task;
    } while (p != &init_task);
}

static int free_area_pages(unsigned long dindex, unsigned long index, unsigned long nr) {
    unsigned long page, *pte;

    if (!(PAGE_PRESENT & (page = swapper_pg_dir[dindex])))
        return 0;
    page &= PAGE_MASK;
    pte = index + (unsigned long *) page;
    do {
        unsigned long pg = *pte;
        *pte = 0;
        if (pg & PAGE_PRESENT)
            free_page(pg);
        pte++;
    } while (--nr);
    pte = (unsigned long *) page;
    for (nr = 0; nr < 1024; nr++, pte++)
        if (*pte)
            return 0;
    set_pgdir(dindex, 0);
    mem_map[MAP_NR(page)] = 1;
    free_page(page);
    invalidate();
    return 0;
}

static int alloc_area_pages(unsigned long dindex, unsigned long index, unsigned long nr) {
    unsigned long page, *pte;

    page = swapper_pg_dir[dindex];
    if (!page) {
        page = get_free_page(GFP_KERNEL);
        if (!page)
            return -ENOMEM;
        if (swapper_pg_dir[dindex]) {
            free_page(page);
            page = swapper_pg_dir[dindex];
        } else {
            mem_map[MAP_NR(page)] = MAP_PAGE_RESERVED;
            set_pgdir(dindex, page | PAGE_SHARED);
        }
    }
    page &= PAGE_MASK;
    pte = index + (unsigned long *) page;
    *pte = PAGE_SHARED;
    do {
        unsigned long pg = get_free_page(GFP_KERNEL);
        if (!pg)
            return -ENOMEM;
        *pte = pg | PAGE_SHARED;
        pte++;
    } while (--nr);
    invalidate();
    return 0;
}

static int do_area(void * addr, unsigned long size,
	int (*area_fn)(unsigned long,unsigned long,unsigned long)) {
    unsigned long nr, dindex, index;

    nr = size >> PAGE_SHIFT;
    dindex = (TASK_SIZE + (unsigned long) addr) >> 22;
    index = (((unsigned long) addr) >> PAGE_SHIFT) & (PTRS_PER_PAGE - 1);
    while (nr > 0) {
        unsigned long i = PTRS_PER_PAGE - index;
        if (i > nr)
            i = nr;
        nr -= i;
        if (area_fn(dindex, index, i))
            return -1;
        index = 0;
        dindex++;
    }
    return 0;
}

void vfree(void * addr) {
    struct vm_struct **p, *tmp;

    if (!addr)
        return;
    if ((PAGE_SIZE - 1) & (unsigned long) addr) {
        printk("Trying to vfree() bad address (%p)\n", addr);
		return;
    }
    for (p = &vmlist; (tmp = *p); p = &tmp->next) {
        if (tmp->addr == addr) {
            *p = tmp->next;
            do_area(tmp->addr, tmp->size, free_area_pages);
            kfree(tmp);
            return;
        }
    }
    printk("Trying to vfree() nonexistent vm area (%p)\n", addr);
}

void * vmalloc(unsigned long size) {
    void *addr;
    struct vm_struct **p, *tmp, *area;

    size = PAGE_ALIGN(size);
    if (!size || size > high_memory)
        return NULL;
    area = (struct vm_struct *) kmalloc(sizeof(*area), GFP_KERNEL);
    if (!area) {
        return NULL;
    }
    // 内核态也是使用虚拟地址空间，high_memory以下是物理内存，以上是虚拟内存，这里隔了8M的虚拟空间
    addr = (void *) ((high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1));
    // 多留的4k空间与上面留的8M作用一样
    area->size = size + PAGE_SIZE;
    area->next = NULL;
    for (p = &vmlist; (tmp = *p); p = &tmp->next) {
        // (*p)->addr是前一个area的起始地址，tmp->addr是后一个area的起始地址
        // 如果加上要分配的size后小于后一个addr，说明
        if (size + (unsigned long) addr < (unsigned long) tmp->addr)
            break;
        addr = (void *) (tmp->size + (unsigned long) tmp->addr);
    }
    area->addr = addr;
    area->next = *p;
    *p = area;
    if (do_area(addr, size, alloc_area_pages)) {
        vfree(addr);
        return NULL;
    }
    return addr;
}