#ifndef _LINUX_PAGE_H
#define _LINUX_PAGE_H

#define PAGE_SHIFT			12
#define PAGE_SIZE			((unsigned long)1<<PAGE_SHIFT)

#ifdef __KERNEL__
#define PAGE_MASK			(~(PAGE_SIZE-1))
#define PAGE_ALIGN(addr)		(((addr)+PAGE_SIZE-1)&PAGE_MASK)

#define PTR_MASK			(~(sizeof(void*)-1))

#define SIZEOF_PTR_LOG2			2

#define PAGE_DIR_OFFSET(base, address)	((unsigned long*)((base)+\
  ((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)*2&PTR_MASK&~PAGE_MASK)))

#define PAGE_PTR(address)		\
  ((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)

#define PTRS_PER_PAGE			(PAGE_SIZE/sizeof(void*))
#endif

#endif