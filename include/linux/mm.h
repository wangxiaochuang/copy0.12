#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <linux/page.h>
#include <linux/sched.h>
#include <linux/kernel.h>

struct vm_area_struct {
	struct task_struct * vm_task;		/* VM area parameters */
	unsigned long vm_start;
	unsigned long vm_end;
	unsigned short vm_page_prot;
	struct vm_area_struct * vm_next;	/* linked list */
	struct vm_area_struct * vm_share;	/* linked list */
	struct inode * vm_inode;
	unsigned long vm_offset;
	struct vm_operations_struct * vm_ops;
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	void (*nopage)(int error_code,
		       struct vm_area_struct * area, unsigned long address);
	void (*wppage)(struct vm_area_struct * area, unsigned long address);
	int (*share)(struct vm_area_struct * from, struct vm_area_struct * to, unsigned long address);
	int (*unmap)(struct vm_area_struct *area, unsigned long, size_t);
};

extern unsigned long __bad_page(void);
extern unsigned long __bad_pagetable(void);
extern unsigned long __zero_page(void);

#define BAD_PAGETABLE __bad_pagetable()
#define BAD_PAGE __bad_page()
#define ZERO_PAGE __zero_page()

extern volatile short free_page_ptr; /* used by malloc and tcp/ip. */

extern int nr_swap_pages;
extern int nr_free_pages;
extern unsigned long free_page_list;
extern int nr_secondary_pages;
extern unsigned long secondary_page_list;

#define MAX_SECONDARY_PAGES 20

extern unsigned long __get_free_page(int priority);

static inline unsigned long get_free_page(int priority) {
	unsigned long page;

	page = __get_free_page(priority);
	if (page)
		__asm__ __volatile__("rep ; stosl"
			: /* no outputs */ \
			:"a" (0),"c" (1024),"D" (page));
	return page;
}

extern void free_page(unsigned long addr);

extern void do_wp_page(unsigned long error_code, unsigned long address,
	struct task_struct *tsk, unsigned long user_esp);
extern void do_no_page(unsigned long error_code, unsigned long address,
	struct task_struct *tsk, unsigned long user_esp);

extern unsigned long paging_init(unsigned long start_mem, unsigned long end_mem);
extern void mem_init(unsigned long low_start_mem,
		     unsigned long start_mem, unsigned long end_mem);
extern void oom(struct task_struct * task);

#define invalidate() \
__asm__ __volatile__("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3"::)

extern unsigned long high_memory;

#define MAP_NR(addr) ((addr) >> PAGE_SHIFT)
#define MAP_PAGE_RESERVED (1<<15)

extern unsigned short * mem_map;

#define PAGE_PRESENT 0x001
#define PAGE_RW 0x002
#define PAGE_USER 0x004
#define PAGE_PWT 0x008
#define PAGE_PCD 0x010
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY 0x040
#define PAGE_COW 0x200

#define PAGE_PRIVATE	(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_ACCESSED | PAGE_COW)
#define PAGE_SHARED	(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_ACCESSED)
#define PAGE_COPY	(PAGE_PRESENT | PAGE_USER | PAGE_ACCESSED | PAGE_COW)
#define PAGE_READONLY	(PAGE_PRESENT | PAGE_USER | PAGE_ACCESSED)
#define PAGE_TABLE	(PAGE_PRESENT | PAGE_RW | PAGE_USER | PAGE_ACCESSED)

#define GFP_BUFFER	0x00
#define GFP_ATOMIC	0x01
#define GFP_USER	0x02
#define GFP_KERNEL	0x03

#endif