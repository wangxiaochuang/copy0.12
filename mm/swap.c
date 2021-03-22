#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/string.h>
// #include <linux/stat.h>

#include <asm/system.h> /* for cli()/sti() */
#include <asm/bitops.h>

extern unsigned long free_page_list;
extern int shm_swap (int);

#define NR_LAST_FREE_PAGES 32
static unsigned long last_free_pages[NR_LAST_FREE_PAGES] = {0,};

unsigned long swap_duplicate(unsigned long entry) {
    return 0;
}

void swap_free(unsigned long entry) {
    
}

void swap_in(unsigned long *table_ptr) {
    
}

asmlinkage int sys_idle(void) {
	need_resched = 1;
	return 0;
}

static int swap_out(unsigned int priority) {
    return 0;
}

static inline void add_mem_queue(unsigned long addr, unsigned long * queue) {

}

static int try_to_free_page(void) {
    int i = 6;
    while (i--) {
        if (shrink_buffers(i))
            return 1;
        if (shm_swap(i))
            return 1;
        if (swap_out(i))
            return 1;
    }
    return 0;
}

void free_page(unsigned long addr) {
    if (addr < high_memory) {
        unsigned short *map = mem_map + MAP_NR(addr);

        if (*map) {
            if (!(*map & MAP_PAGE_RESERVED)) {
                unsigned long flag;

                save_flags(flag);
                cli();
                if (!--*map) {
                    if (nr_secondary_pages < MAX_SECONDARY_PAGES) {
                        add_mem_queue(addr, &secondary_page_list);
                        nr_secondary_pages++;
                        restore_flags(flag);
                        return;
                    }
                    add_mem_queue(addr, &free_page_list);
                    nr_free_pages++;
                }
                restore_flags(flag);
            }
            return;
        }
        printk("Trying to free free memory (%08lx): memory probabably corrupted\n",addr);
		printk("PC = %08lx\n",*(((unsigned long *)&addr)-1));
		return;
    }
}

/**
 * free_page_list是4k对齐的
 **/
#define REMOVE_FROM_MEM_QUEUE(queue,nr) \
    cli(); \
    if ((result = queue) != 0) { \
        if (!(result & ~PAGE_MASK) && result < high_memory) { \
            queue = *(unsigned long *) result; \
            if (!mem_map[MAP_NR(result)]) { \
                mem_map[MAP_NR(result)] = 1; \
                nr--; \
                last_free_pages[index = (index + 1) & (NR_LAST_FREE_PAGES - 1)] = result; \
                restore_flags(flag); \
                return result; \
            } \
            printk("Free page %08lx has mem_map = %d\n", \
				result, mem_map[MAP_NR(result)]); \
        } else \
            printk("Result = 0x%08lx - memory map destroyed\n", result); \
        queue = 0; \
        nr = 0; \
    } else if (nr) { \
        printk(#nr " is %d, but " #queue " is empty\n",nr); \
        nr = 0; \
    } \
    restore_flags(flag);

unsigned long __get_free_page(int priority) {
    extern unsigned long intr_count;
    unsigned long result, flag;
    static unsigned long index = 0;

    // 硬件中断期间，标志需要是GFP_ATOMIC
    if (intr_count && priority != GFP_ATOMIC) {
        printk("gfp called nonatomically from interrupt %08lx\n",
			((unsigned long *)&priority)[-1]);
        priority = GFP_ATOMIC;
    }
    save_flags(flag);
repeat:
    REMOVE_FROM_MEM_QUEUE(free_page_list, nr_free_pages);
    // free_page_list没有空闲页了才会进入下面
    if (priority == GFP_BUFFER)
        return 0;
    if (priority != GFP_ATOMIC)
        if (try_to_free_page())
            goto repeat;
    REMOVE_FROM_MEM_QUEUE(secondary_page_list, nr_secondary_pages);
    return 0;
}