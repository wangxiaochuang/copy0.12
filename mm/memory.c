#include <linux/mm.h>

unsigned long HIGH_MEMORY = 0;

/* 15M / 4k from 1M begin */
unsigned char mem_map [ PAGING_PAGES ] = {0, };

/**
 * 4M - 16M
 */
void mem_init(long start_mem, long end_mem) {
    int i;
    HIGH_MEMORY = end_mem;
    for (i = 0; i < PAGING_PAGES; i++) {
        mem_map[i] = USED;
    }
    
    i = MAP_NR(start_mem);
    /* number of main memory */
    end_mem -= start_mem
    end_mem >>= 12
    while (end_mem-- > 0) {
        mem_map[i++] = 0
    }
}
