#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

#include <linux/kernel.h>

extern int SWAP_DEV;

#define read_swap_page(nr,buffer) ll_rw_page(READ,SWAP_DEV,(nr),(buffer));
#define write_swap_page(nr,buffer) ll_rw_page(WRITE,SWAP_DEV,(nr),(buffer));

extern unsigned long get_free_page(void);
extern void free_page(unsigned long addr);
extern void init_swapping(void);

static inline void oom(void) {
	printk("out of memory\n\r");
	while(1) { printk("out of memory"); };
}

#define invalidate() __asm__("movl %%eax,%%cr3"::"a" (0))

#define LOW_MEM 0x100000
extern unsigned long HIGH_MEMORY;
#define PAGING_MEMORY (15*1024*1024)
#define PAGING_PAGES (PAGING_MEMORY>>12)
#define LOW_MEM 0x100000
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)
#define USED 100

extern unsigned char mem_map [ PAGING_PAGES ];

#endif
