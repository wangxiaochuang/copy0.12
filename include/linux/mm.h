#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

#include <linux/kernel.h>

extern unsigned long get_free_page(void);
extern void free_page(unsigned long addr);

static inline void oom(void)
{
	printk("out of memory\n\r");
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
