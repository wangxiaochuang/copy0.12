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

// 任务0占据了前64MB线性空间，所以从任务1的开始位置开始才做虚拟内存交换，内核的内存不做交换
// 64M >> 12
#define FIRST_VM_PAGE (TASK_SIZE >> 12)

// 最后一个任务 4G >> 12
#define LAST_VM_PAGE (1024 * 1024)

// 除去任务0以外的其他任务的所有页数目
#define VM_PAGES (LAST_VM_PAGE - FIRST_VM_PAGE)

static int get_swap_page(void) {
    int nr;
    if (!swap_bitmap) {
        return 0;
    }
    for (nr = 1; nr < SWAP_BITS; nr++) {
        // 复位nr位置的比特位，成功则返回nr
        if (clrbit(swap_bitmap, nr)) {
            return nr;
        }
    }
    return 0;
}

void swap_free(int swap_nr) {
    if (!swap_nr) {
        return;
    }
    if (swap_bitmap && swap_nr < SWAP_BITS) {
        if (!setbit(swap_bitmap, swap_nr)) {
            return;
        }
    }
    printk("Swap-space bad (swap_free())\n\r");
    return;
}

// 尝试交换这一页
int try_to_swap_out(unsigned long *table_ptr) {
    unsigned long page;
    unsigned long swap_nr;

    page = *table_ptr;
    // 要交换的页不存在直接退出进入下一个
    if (!(PAGE_PRESENT & page)) {
        return 0;
    }
    // 指定物理内存地址高于内存高端或低于LOW_MEM
    if (page - LOW_MEM > PAGING_MEMORY) {
        return 0;
    }
    // 内存页面已被修改过
    if (PAGE_DIRTY & page) {
        // 页物理地址
        page &= 0xfffff000;
        if (mem_map[MAP_NR(page)] != 1) {       // 页面是被共享的，不宜换出
            return 0;
        }
        if (!(swap_nr = get_swap_page())) {
            return 0;
        }
        // 左移一位，控制页存在位，P=0;之前存物理页地址，现在存交换磁盘的索引
        *table_ptr = swap_nr << 1;
        invalidate();
        write_swap_page(swap_nr, (char *) page);
        free_page(page);
        return 1;
    }
    // 页面没有被修改过，直接释放
    *table_ptr = 0;
    invalidate();
    free_page(page);
    return 1;
}

int swap_out(void) {
    // 任务1的起始地址对应的页目录项，64M >> 22 = 16
    static int dir_entry = FIRST_VM_PAGE >> 10;
    static int page_entry = -1;
    int counter = VM_PAGES;
    int pg_table;

    while (counter > 0) {
        // 页目录数组，每一个任务可以占用64个页目录项
        pg_table = pg_dir[dir_entry];
        // 找到第一个有效的页目录项
        if (pg_table & 1) {
            break;
        }
        counter -= 1024;
        dir_entry++;
        if (dir_entry >= 1024) {
            dir_entry = FIRST_VM_PAGE >> 10;
        }
    }
    // 从找到的页目录项对应页表的第一项开始尝试逐一交换
    pg_table &= 0xfffff000;
    while (counter-- > 0) {
        page_entry++;
        // 该页目录项的所有1024项都没有交换成功，开始找下一个页目录项
        if (page_entry >= 1024) {
            page_entry = 0;
        repeat:
            // 下一个页目录项
            dir_entry++;
            // 页目录项的所有项都遍历完了，重置
            if (dir_entry >= 1024) {
                dir_entry = FIRST_VM_PAGE >> 10;
            }
            pg_table = pg_dir[dir_entry];
            // 页目录项不存在继续下一个
            if (!(pg_table & 1)) {
                if ((counter -= 1024) > 0) {
                    goto repeat;
                } else {
                    break;
                }
            }
            // 找到的页目录项的第一项地址
            pg_table &= 0xfffff000;
        }
        if (try_to_swap_out(page_entry + (unsigned long *) pg_table)) {
            return 1;
        }
    }
    printk("Out of swap-memory\n\r");
    return 0;
}

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
    if (!__res && swap_out()) {	/* 没有得到空闲页面则执行交换处理,并重新查找 */
        goto repeat;
    }
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
    if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10)) {
        panic("Unable to find swap-space signature\n\r");
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        return;
    }
    memset(swap_bitmap + 4086, 0, 10);
    for (i = 0; i < SWAP_BITS; i++) {
        if (i == 1) {
            i = swap_size;
        }
        if (bit(swap_bitmap, i)) {
            panic("Bad swap-space bits: %d bit-map %d\n\r", swap_size, i);
            free_page((long) swap_bitmap);
            swap_bitmap = NULL;
            return;
        }
    }
    j = 0;
    for (i = 1; i < swap_size; i++) {
        if (bit(swap_bitmap, i))
            j++;
    }
    if (!j) {
        free_page((long) swap_bitmap);
        swap_bitmap = NULL;
        printk("has no more swap bit\n\r");
        return;
    }
    printk("Swap device ok: %d pages (%d bytes) swap-space\n\r", j, j*4096);
}