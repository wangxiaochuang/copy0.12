#include <string.h>
#include <linux/sched.h>

/* 1页(4096B)共有32768个位。最多可管理32768个页面，对应128MB内存容量 */
#define SWAP_BITS (4096 << 3)

#define bitop(name, op) 								\
static inline int name(char * addr, unsigned int nr) 	\
{ 														\
    int __res; 											\
    __asm__ __volatile__("bt" op " %1, %2; adcl $0, %0" \
        :"=g" (__res) 									\
        :"r" (nr),"m" (*(addr)),"0" (0)); 				\
    return __res; 										\
}
bitop(bit, "")
bitop(setbit, "s")
bitop(clrbit, "r")

static char * swap_bitmap = NULL;
int SWAP_DEV = 0;

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

void init_swapping(void) {
    extern int *blk_size[];
    int swap_size, i, j;

    if (!SWAP_DEV) {
        return;
    }
    if (!blk_size[MAJOR(SWAP_DEV)]) {
        panic("Unable to get size of swap device\n\r");
    }
    swap_size = blk_size[MAJOR(SWAP_DEV)][MINOR(SWAP_DEV)];
    if (!swap_size) {
        return;
    }
    if (swap_size < 100)
        panic("Swap device too small (%d blocks)\n\r", swap_size);
    // 4KB 为单位
    swap_size >>= 2;
    if (swap_size > SWAP_BITS) {
        swap_size = SWAP_BITS;
    }
    swap_bitmap = (char *) get_free_page();
    if (!swap_bitmap) {
        printk("Unable to start swapping: out of memory :-)\n\r");
        return;
    }
    read_swap_page(0, swap_bitmap);
    /*
    if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10)) {
        panic("Unable to find swap-space signature\n\r");
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    */
    memset(swap_bitmap + 4086, 0, 10);
    for (i = 0; i < SWAP_BITS; i++) {
        if (i == 1) {
            i = swap_size;
        }
        if (bit(swap_bitmap, i)) {
            panic("Bad swap-space bit-map\n\r");
            free_page((long) swap_bitmap);
            swap_bitmap = NULL;
            return;
        }
    }
    j = 0;
    for (i = 1; i < swap_size; i++) {
        // 无交换分区，因此手动设置为可用, @todo
        setbit(swap_bitmap, i);
        if (bit(swap_bitmap, i))
            j++;
    }
    if (!j) {
        panic("j is 0\n\r");
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    printk("Swap device ok: %d pages (%d bytes) swap-space\n\r", j, j*4096);
}