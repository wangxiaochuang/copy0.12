#ifndef _LINUX_PAGE_H
#define _LINUX_PAGE_H

#define PAGE_SHIFT			12
#define PAGE_SIZE			((unsigned long)1<<PAGE_SHIFT)

#ifdef __KERNEL__
#define PAGE_MASK			(~(PAGE_SIZE-1))
#define PAGE_ALIGN(addr)		(((addr)+PAGE_SIZE-1)&PAGE_MASK)
#define PTRS_PER_PAGE			(PAGE_SIZE/sizeof(void*))
#endif

#endif