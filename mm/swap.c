#include <string.h>
#include <errno.h>

#include <sys/stat.h>
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
unsigned int swap_device = 0;
struct inode *swap_file = NULL;

void rw_swap_page(int rw, unsigned int nr, char *buf) {
    unsigned int zones[4];
    int i;

    if (swap_device) {
        ll_rw_page(rw, swap_device, nr, buf);
        return;
    }
    if (swap_file) {
        // swap里的nr表示第几个页面，因为一个一个页面是4k，所以这里要乘以4
        nr <<= 2;
        for (i = 0; i < 4; i++) {
            if (!(zones[i] = bmap(swap_file, nr++))) {
                printk("rw_swap_page: bad swap file\n");
                return;
            }
        }
        ll_rw_swap_file(rw, swap_file->i_dev, zones, 4, buf);
        return;
    }
    printk("ll_swap_page: no swap file or device\n");
}

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
    printk("swap_free: swap-space bitmap bad\n");
    return;
}

void swap_in(unsigned long *table_ptr) {
    int swap_nr;
    unsigned long page;

    if (!swap_bitmap) {
        printk("Trying to swap in without swap bit-map");
        return;
    }
    if (1 & *table_ptr) {
        printk("trying to swap in present page\n\r");
        return;
    }
    swap_nr = *table_ptr >> 1;
    if (!swap_nr) {
        printk("No swap page in swap_in\n\r");
        return;
    }
    if (!(page = get_free_page())) {
        oom();
    }
    read_swap_page(swap_nr, (char *) page);
    if (setbit(swap_bitmap, swap_nr)) {
        printk("swapping in multiply from same page\n\r");
    }
    *table_ptr = page | (PAGE_DIRTY | 7);
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
    // 要交换的地址高于能管理的最大地址
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
        // 要准备将page内存页交换出去
        if (!(swap_nr = get_swap_page())) {
            return 0;
        }
        // 左移一位，控制页存在位，P=0;之前存物理页地址，现在存交换分区位图的索引
        *table_ptr = swap_nr << 1;
        invalidate();
        write_swap_page(swap_nr, (char *) page);
        free_page(page);
        return 1;
    }
    page &= 0xfffff000;
    // 页面没有被修改过，直接释放
    *table_ptr = 0;
    invalidate();
    free_page(page);
    return 1;
}

int swap_out(void) {
    // 任务1的起始地址对应的页目录项，64M >> 22 = 16
    static int dir_entry = 1024;
    static int page_entry = -1;
    // 除了任务0的其他任务，是从64M开始的
    int counter = VM_PAGES;
    int pg_table;
check_dir:
    if (counter < 0)
        goto no_swap;
    if (dir_entry >= 1024)
        // 64M >> 12 >> 10 就是页目录索引
        dir_entry = FIRST_VM_PAGE >> 10;
    // 这个页目录项不存在
    if (!(1 & (pg_table = pg_dir[dir_entry]))) {
        if (pg_table) {
            printk("bad page-table at pg_dir[%d]: %08x\n\r",
				dir_entry, pg_table);
			pg_dir[dir_entry] = 0;
        }
        // 剩余页目录项的页表项总和
        counter -= 1024;
        // 下一个页目录索引
        dir_entry++;
        goto check_dir;
    }
    pg_table &= 0xfffff000;
check_table:
    if (counter < 0)
        goto no_swap;
    counter--;
    page_entry++;
    // 这一个页目录项处理完了
    if (page_entry >= 1024) {
        page_entry = -1;
        dir_entry++;
        goto check_dir;
    }
    // 处理每一个页表项，尝试交换出去
    if (try_to_swap_out(page_entry + (unsigned long *) pg_table)) {
        // 64M >> 12 >> 10 >> 4 => 就是进程号
        if (!task[dir_entry >> 4])
            printk("swapping out page from non-existent task\n\r");
        else
            task[dir_entry >> 4]->rss--;
        return 1;
    }
    goto check_table;
no_swap:
    printk("Out of swap-memory\n\r");
	return 0;
}

unsigned long get_free_page(void) {
    unsigned long result;
repeat:
    __asm__("std ; repne ; scasb\n\t" // 从mem_map的最后开始往前扫描第一个等于0的位子
        "jne 1f\n\t"
        "movb $1,1(%%edi)\n\t"        // 找到了，将meme_map的对应位置置1（edi每次循环都在减1字节）
        "sall $12,%%ecx\n\t"          // ecx表示了mem_map的序号，乘以4096得到相对地址
        "addl %2,%%ecx\n\t"           // 加上低端地址1M得到真实物理地址
        "movl %%ecx,%%edx\n\t"
        "movl $1024,%%ecx\n\t"        // 循环1024次，每次处理4个字节，将页面清零（eax=0）
        "leal 4092(%%edx),%%edi\n\t"
        "rep ; stosl\n\t"
        "movl %%edx,%%eax\n"
        "1:\tcld"
        :"=a" (result)
        :"0" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),
        "D" (mem_map + PAGING_PAGES - 1)
        );
    if (result >= HIGH_MEMORY) {	/* 页面地址大于实际内存容量，重新寻找 */
        goto repeat;
    }
    if ((result && result < LOW_MEM || (result & 0xfff))) {
        printk("weird result: %08x\n", result);
		result = 0;
    }
    if (!result && swap_out()) {	/* 没有得到空闲页面则执行交换处理,并重新查找 */
        goto repeat;
    }
    return result;
}

int sys_swapon(const char *specialfile) {
    struct inode *swap_inode;
    int i, j;

    if (!super())
        return -EPERM;
    if (!(swap_inode = namei(specialfile)))
        return -ENOENT;
    if (swap_file || swap_device || swap_bitmap) {
        iput(swap_inode);
        return -EBUSY;
    }
    if (S_ISBLK(swap_inode->i_mode)) {
        swap_device = swap_inode->i_rdev;
        iput(swap_inode);
    } else if (S_ISREG(swap_inode->i_mode)) {
        swap_file = swap_inode;
    } else {
        iput(swap_inode);
        return -EINVAL;
    }
    swap_bitmap = (char *) get_free_page();
    if (!swap_bitmap) {
        iput(swap_file);
        swap_device = 0;
        swap_file = NULL;
        printk("Unable to start swapping: out of memory :-)\n");
		return -ENOMEM;
    }
    read_swap_page(0, swap_bitmap);
    if (strncmp("SWAP-SPACE", swap_bitmap + 4086, 10)) {
        printk("Unable to find swap-space signature\n\r");
		free_page((long) swap_bitmap);
		iput(swap_file);
		swap_device = 0;
		swap_file = NULL;
		swap_bitmap = NULL;
		return -EINVAL;
    }
    memset(swap_bitmap + 4086, 0, 10);
    j = 0;
    for (i = 1; i < SWAP_BITS; i++)
        if (bit(swap_bitmap, i))
            j++;
    if (!j) {
        printk("Empty swap-file\n");
		free_page((long) swap_bitmap);
		iput(swap_file);
		swap_device = 0;
		swap_file = NULL;
		swap_bitmap = NULL;
		return -EINVAL;
    }
    printk("Adding Swap: %d pages (%d bytes) swap-space\n\r", j, j*4096);
	return 0;
}