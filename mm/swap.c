#include <linux/sched.h>

unsigned long get_free_page(void) {
    register unsigned long __res;
repeat:
    __asm__("std ; repne ; scasb\n\t"
        "jne 1f\n\t"
        "movb $1,1(%%edi)\n\t"
        "sall $12,%%ecx\n\t"
        "addl %2,%%ecx\n\t"
        "movl %%ecx,%%edx\n\t"
        "movl $1024,%%ecx\n\t"
        "leal 4092(%%edx),%%edi\n\t"
        "rep ; stosl\n\t"
        "movl %%edx,%%eax\n"
        "1:"
        :"=a" (__res)
        :"0" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),
        "D" (mem_map + PAGING_PAGES - 1)
        );
    if (__res >= HIGH_MEMORY) {	/* 页面地址大于实际内存容量，重新寻找 */
        goto repeat;
    }
    // if (!__res && swap_out()) {	/* 没有得到空闲页面则执行交换处理,并重新查找 */
        // goto repeat;
    // }
    if (!__res) {
        goto repeat;
    }
    return __res;
}