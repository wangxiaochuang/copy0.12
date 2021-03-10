#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

#include <linux/kernel.h>
#include <signal.h>

extern unsigned int swap_device;
extern struct inode *swap_file;

extern void rw_swap_page(int rw, unsigned int nr, char *buf);

#define read_swap_page(nr,buf) \
	rw_swap_page(READ,(nr),(buf))
#define write_swap_page(nr,buf) \
	rw_swap_page(WRITE,(nr),(buf))

extern unsigned long get_free_page(void);
extern unsigned long put_dirty_page(unsigned long page,unsigned long address);
extern void free_page(unsigned long addr);
extern void init_swapping(void);
void swap_free(int page_nr);
void swap_in(unsigned long *table_ptr);

static inline void oom(void) {
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
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

#define PAGE_DIRTY		0x40		/* 脏位 */
#define PAGE_ACCESSED	0x20		/* 已访问位 */
#define PAGE_USER		0x04		/* 用户/超级用户位 */
#define PAGE_RW			0x02		/* 页面读写位 */
#define PAGE_PRESENT	0x01		/* 页面存在位 */

#endif
