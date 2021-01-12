#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

#define PAGING_MEMORY (15*1024*1024)
#define PAGING_PAGES (PAGING_MEMORY>>12)
#define LOW_MEM 0x100000
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)
#define USED 100

#endif
