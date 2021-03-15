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

extern unsigned long paging_init(unsigned long start_mem, unsigned long end_mem);

#define invalidate() \
__asm__ __volatile__("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3"::)

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

#endif