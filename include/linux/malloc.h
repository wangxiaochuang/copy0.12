#ifndef _LINUX_MALLOC_H
#define _LINUX_MALLOC_H

#include <linux/config.h>

#ifdef CONFIG_DEBUG_MALLOC
#else

void * kmalloc(unsigned int size, int priority);
void kfree_s(void * obj, int size);

#define kcheck_s(a,b) 0

#define kfree(x) kfree_s((x), 0)
#define kcheck(x) kcheck_s((x), 0)

#endif

#endif