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

static inline void add_mem_queue(unsigned long addr, unsigned long * queue) {

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

unsigned long __get_free_page(int priority) {
    return 0;
}