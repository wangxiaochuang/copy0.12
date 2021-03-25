#include <asm/system.h>
#include <linux/config.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>

unsigned long high_memory = 0;

extern unsigned long pg0[1024];

extern void die_if_kernel(char *,struct pt_regs *,long);

int nr_free_pages = 0;
unsigned long free_page_list = 0;

int nr_secondary_pages = 0;
unsigned long secondary_page_list = 0;

#define copy_page(from,to) \
__asm__("cld ; rep ; movsl": :"S" (from),"D" (to),"c" (1024))

unsigned short * mem_map = NULL;

#define CODE_SPACE(addr,p) ((addr) < (p)->end_code)

void oom(struct task_struct * task) {
	printk("\nout of memory\n");
	task->sigaction[SIGKILL-1].sa_handler = NULL;
	task->blocked &= ~(1<<(SIGKILL-1));
	send_sig(SIGKILL,task,1);
}

static void free_one_table(unsigned long * page_dir) {
    int j;
	unsigned long pg_table = *page_dir;
	unsigned long * page_table;

	if (!pg_table)
		return;
	*page_dir = 0;
    if (pg_table >= high_memory || !(pg_table & PAGE_PRESENT)) {
		printk("Bad page table: [%p]=%08lx\n", page_dir, pg_table);
		return;
	}
    if (mem_map[MAP_NR(pg_table)] & MAP_PAGE_RESERVED)
        return;
    page_table = (unsigned long *) (pg_table & PAGE_MASK);
    for (j = 0; j < PTRS_PER_PAGE; j++, page_table++) {
        unsigned long pg = *page_table;
        if (!pg)
            continue;
        *page_table = 0;
        if (pg & PAGE_PRESENT)
            free_page(PAGE_MASK & pg);
        else
            swap_free(pg);
    }
}

void free_page_tables(struct task_struct * tsk) {
    int i;
	unsigned long pg_dir;
	unsigned long * page_dir;

    if (!tsk)
        return;
    if (tsk == task[0]) {
		printk("task[0] (swapper) killed: unable to recover\n");
		panic("Trying to free up swapper memory space");
	}
	pg_dir = tsk->tss.cr3;
    if (!pg_dir || pg_dir == (unsigned long) swapper_pg_dir) {
        printk("Trying to free kernel page-directory: not good\n");
		return;
    }
    tsk->tss.cr3 = (unsigned long) swapper_pg_dir;
    if (tsk == current)
        __asm__ __volatile__("movl %0,%%cr3": :"a" (tsk->tss.cr3));
    if (mem_map[MAP_NR(pg_dir)] > 1) {
        free_page(pg_dir);
        return;
    }
    page_dir = (unsigned long *) pg_dir;
    for (i = 0; i < PTRS_PER_PAGE; i++, page_dir++)
        free_one_table(page_dir);
    free_page(pg_dir);
    invalidate();
}

int clone_page_tables(struct task_struct * tsk) {
    unsigned long pg_dir;

    pg_dir = current->tss.cr3;
    mem_map[MAP_NR(pg_dir)]++;
    tsk->tss.cr3 = pg_dir;
    return 0;
}

int copy_page_tables(struct task_struct * tsk) {
    int i;
	unsigned long old_pg_dir, *old_page_dir;
	unsigned long new_pg_dir, *new_page_dir;

    if (!(new_pg_dir = get_free_page(GFP_KERNEL)))
        return -ENOMEM;
    old_pg_dir = current->tss.cr3;
    tsk->tss.cr3 = new_pg_dir;
    old_page_dir = (unsigned long *) old_pg_dir;
    new_page_dir = (unsigned long *) new_pg_dir;
    for (i = 0; i < PTRS_PER_PAGE; i++, old_page_dir++, new_page_dir++) {
        int j;
        unsigned long old_pg_table, *old_page_table;
        unsigned long new_pg_table, *new_page_table;

        old_pg_table = *old_page_dir;
        if (!old_pg_table)
            continue;
        if (old_pg_table >= high_memory || !(old_pg_table & PAGE_PRESENT)) {
            printk("copy_page_tables: bad page table: "
				"probable memory corruption");
            *old_page_dir = 0;
            continue;
        }
        if (mem_map[MAP_NR(old_pg_table)] & MAP_PAGE_RESERVED) {
            *new_page_dir = old_pg_table;
            continue;
        }
        if (!(new_pg_table = get_free_page(GFP_KERNEL))) {
            free_page_tables(tsk);
            return -ENOMEM;
        }
        old_page_table = (unsigned long *) (PAGE_MASK & old_pg_table);
        new_page_table = (unsigned long *) (PAGE_MASK & new_pg_table);
        for (j = 0; j < PTRS_PER_PAGE; j++, old_page_table++, new_page_table++) {
            unsigned long pg;
            pg = *old_page_table;
            if (!pg)
                continue;
            if (!(pg & PAGE_PRESENT)) {
                *new_page_table = swap_duplicate(pg);
                continue;
            }
            if ((pg & (PAGE_RW | PAGE_COW)) == (PAGE_RW | PAGE_COW))
                pg &= ~PAGE_RW;
            *new_page_table = pg;
            if (mem_map[MAP_NR(pg)] & MAP_PAGE_RESERVED)
                continue;
            *old_page_table = pg;
            mem_map[MAP_NR(pg)]++;
        }
        *new_page_dir = new_pg_table | PAGE_TABLE;
    }
    invalidate();
    return 0;
}

unsigned long put_page(struct task_struct * tsk,unsigned long page,
	unsigned long address,int prot) {
    unsigned long *page_table;

    if ((prot & (PAGE_MASK | PAGE_PRESENT)) != PAGE_PRESENT)
        printk("put_page: prot = %08x\n", prot);
    if (page >= high_memory) {
        printk("put_page: trying to put page %08lx at %08lx\n",page,address);
		return 0;
    }
    page_table = PAGE_DIR_OFFSET(tsk->tss.cr3, address);
    if ((*page_table) & PAGE_PRESENT)
        page_table = (unsigned long *) (PAGE_MASK & *page_table);
    else {
        printk("put_page: bad page directory entry\n");
        oom(tsk);
        *page_table = BAD_PAGETABLE | PAGE_TABLE;
        return 0;
    }
    page_table += (address >> PAGE_SHIFT) & (PTRS_PER_PAGE - 1);
    if (*page_table) {
        printk("put_page: page already exists\n");
        *page_table = 0;
        invalidate();
    }
    *page_table = page | prot;
    return page;
}

unsigned long put_dirty_page(struct task_struct * tsk, unsigned long page, unsigned long address) {
    unsigned long tmp, *page_table;

    if (page >= high_memory)
        printk("put_dirty_page: trying to put page %08lx at %08lx\n", page, address);
    return 0;
}

static void __do_wp_page(unsigned long error_code, unsigned long address,
	struct task_struct * tsk, unsigned long user_esp) {
    unsigned long *pde, pte, old_page, prot;
	unsigned long new_page;

    new_page = __get_free_page(GFP_KERNEL);
    pde = PAGE_DIR_OFFSET(tsk->tss.cr3, address);
    pte = *pde;
    if (!(pte & PAGE_PRESENT))
        goto end_wp_page;
    if ((pte & PAGE_TABLE) != PAGE_TABLE || pte >= high_memory)
        goto bad_wp_pagetable;
    pte &= PAGE_MASK;
    pte += PAGE_PTR(address);
    old_page = *(unsigned long *) pte;
    if (!(old_page & PAGE_PRESENT))
        goto end_wp_page;
    if (old_page >= high_memory)
        goto bad_wp_page;
    if (old_page & PAGE_RW)
        goto end_wp_page;
    tsk->min_flt++;
    prot = (old_page | ~PAGE_MASK) | PAGE_RW;
    old_page &= PAGE_MASK;
    if (mem_map[MAP_NR(old_page)] != 1) {
        if (new_page) {
            if (mem_map[MAP_NR(old_page)] & MAP_PAGE_RESERVED)
                ++tsk->rss;
            copy_page(old_page, new_page);
            *(unsigned long *) pte = new_page | prot;
            free_page(old_page);
            invalidate();
            return;
        }
        free_page(old_page);
        oom(tsk);
        *(unsigned long *) pte = BAD_PAGE | prot;
		invalidate();
		return;
    }
    *(unsigned long *) pte |= PAGE_RW;
    invalidate();
    if (new_page)
        free_page(new_page);
    return;
    
bad_wp_page:
	printk("do_wp_page: bogus page at address %08lx (%08lx)\n",address,old_page);
	*(unsigned long *) pte = BAD_PAGE | PAGE_SHARED;
	send_sig(SIGKILL, tsk, 1);
	goto end_wp_page;

bad_wp_pagetable:
	printk("do_wp_page: bogus page-table at address %08lx (%08lx)\n",address,pte);
	*pde = BAD_PAGETABLE | PAGE_TABLE;
	send_sig(SIGKILL, tsk, 1);

end_wp_page:
	if (new_page)
		free_page(new_page);
	return;
}

void do_wp_page(unsigned long error_code, unsigned long address,
	struct task_struct * tsk, unsigned long user_esp) {
    unsigned long page;
    unsigned long *pg_table;

    pg_table = PAGE_DIR_OFFSET(tsk->tss.cr3, address);
    page = *pg_table;
    // 页目录表中压根不存在
    if (!page)
        return;
    if ((page & PAGE_PRESENT) && page < high_memory) {
        // address在页表中的地址
        pg_table = (unsigned long *) ((page & PAGE_MASK) + PAGE_PTR(address));
        page = *pg_table;
        // 要写保护的页面不存在
        if (!(page & PAGE_PRESENT))
            return;
        // 要写保护的页面本来就可写
        if (page & PAGE_RW)
            return;
        // 必须有明确的写时复制的标记
        if (!(page & PAGE_COW)) {
            if (user_esp && tsk == current) {
                current->tss.cr2 = address;
                current->tss.error_code = error_code;
                current->tss.trap_no = 14;
                send_sig(SIGSEGV, tsk, 1);
                return;
            }
        }
        // 直接改为可写状态
        if (mem_map[MAP_NR(page)] == 1) {
            *pg_table |= PAGE_RW | PAGE_DIRTY;
            invalidate();
            return;
        }
        // 复制一页内存
        __do_wp_page(error_code, address, tsk, user_esp);
        return;
    }
    printk("bad page directory entry %08lx\n",page);
	*pg_table = 0;
}

int __verify_write(unsigned long start, unsigned long size) {
    size--;
    size += start & ~PAGE_MASK;
    size >>= PAGE_SHIFT;
    start &= PAGE_MASK;
    do {
        do_wp_page(1, start, current, 0);
        start += PAGE_SIZE;
    } while (size--);
    return 0;
}

static inline void get_empty_page(struct task_struct * tsk, unsigned long address) {
    unsigned long tmp;
    if (!(tmp = get_free_page(GFP_KERNEL))) {
        oom(tsk);
        tmp = BAD_PAGE;
    }
    if (!put_page(tsk, tmp, address, PAGE_PRIVATE))
        free_page(tmp);
}

static inline unsigned long get_empty_pgtable(struct task_struct *tsk, unsigned long address) {
    unsigned long page;
    unsigned long *p;

    p = PAGE_DIR_OFFSET(tsk->tss.cr3, address);
    if (PAGE_PRESENT & *p)
        return *p;
    // 页目录项如果分配了，其存在位一定为1，否则就是异常的
    if (*p) {
        printk("get_empty_pgtable: bad page-directory entry \n");
        *p = 0;
    }
    // 分配页表
    page = get_free_page(GFP_KERNEL);
    // 分配过程中可能其页表又存在了，所以二次检测
    p = PAGE_DIR_OFFSET(tsk->tss.cr3, address);
    if (PAGE_PRESENT & *p) {
        free_page(page);
        return *p;
    }
    if (*p) {
        printk("get_empty_pgtable: bad page-directory entry \n");
		*p = 0;
    }
    if (page) {
        *p = page | PAGE_TABLE;
        return *p;
    }
    oom(current);
    *p = BAD_PAGETABLE | PAGE_TABLE;
    return 0;
}

void do_no_page(unsigned long error_code, unsigned long address,
	struct task_struct *tsk, unsigned long user_esp) {
    unsigned long tmp;
    unsigned long page;
    struct vm_area_struct *mpnt;

    page = get_empty_pgtable(tsk, address);
    if (!page)
        return;
    page &= PAGE_MASK;
    // address在页表中的地址
    page += PAGE_PTR(address);
    // address在页表中的条目内容
    tmp = *(unsigned long *) page;
    if (tmp & PAGE_PRESENT)
        return;
    ++tsk->rss;
    // 在交换分区中
    if (tmp) {
        ++tsk->maj_flt;
        swap_in((unsigned long *) page);
        return;
    }
    // 物理页不存在
    address &= 0xfffff000;
    tmp = 0;
    for (mpnt = tsk->mmap; mpnt != NULL; mpnt = mpnt->vm_next) {
        if (address < mpnt->vm_start)
            break;
        if (address >= mpnt->vm_end) {
            tmp = mpnt->vm_end;
            continue;
        }
        // address在某个已分配的虚拟地址区域中
        if (!mpnt->vm_ops || !mpnt->vm_ops->nopage) {
            ++tsk->min_flt;
            get_empty_page(tsk, address);
            return;
        }
        mpnt->vm_ops->nopage(error_code, mpnt, address);
        return;
    }
    if (tsk != current)
        goto ok_no_page;
    // 堆的地址空间
    if (address >= tsk->end_data && address < tsk->brk)
        goto ok_no_page;
    // 如果是在用户栈区域
    // 确保栈延长的长度足够
    // 确保增长后的栈长度小于其限额
    if (mpnt && mpnt == tsk->stk_vma &&
        address - tmp > mpnt->vm_start - address &&
        tsk->rlim[RLIMIT_STACK].rlim_cur > mpnt->vm_end - address) {
        mpnt->vm_start = address;
        goto ok_no_page;
    }
    tsk->tss.cr2 = address;
    current->tss.error_code = error_code;
    current->tss.trap_no = 14;
    send_sig(SIGSEGV, tsk, 1);
    if (error_code & 4)
        return;
ok_no_page:
    ++tsk->min_flt;
    get_empty_page(tsk, address);
}

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code) {
    unsigned long address;
    unsigned long user_esp = 0;
    unsigned int bit;

    __asm__("movl %%cr2, %0":"=r" (address));
    // 用户态地址会小于TASK_SIZE
    if (address < TASK_SIZE) {
        // 如果位2置位表示正在执行用户代码
        if (error_code & 4) {
            if (regs->eflags & VM_MASK) {
                bit = (address - 0xA0000) >> PAGE_SHIFT;
                if (bit < 32)
                    current->screen_bitmap |= 1 << bit;
            } else
                user_esp = regs->esp;
        }
        // 为1表示违反页保护引发异常
        if (error_code & 1)
            do_wp_page(error_code, address, current, user_esp);
        else
            do_no_page(error_code, address, current, user_esp);
        return;
    }
    // 内核态的地址
    address -= TASK_SIZE;
    if (wp_works_ok < 0 && address == 0 && (error_code & PAGE_PRESENT)) {
        wp_works_ok = 1;
        pg0[0] = PAGE_SHARED;
        printk("This processor honours the WP bit even when in supervisor mode. Good.\n");
		return;
    }
    // 内核代码就不应该引发写保护异常
    if (address < PAGE_SIZE) {
        printk("Unable to handle kernel NULL pointer dereference");
		pg0[0] = PAGE_SHARED;
    } else
        printk("Unable to handle kernel paging request");
    printk(" at address %08lx\n",address);
    die_if_kernel("Oops", regs, error_code);
    do_exit(SIGKILL);
}

unsigned long __bad_pagetable(void) {
    extern char empty_bad_page_table[PAGE_SIZE];

    __asm__ __volatile__("cld; rep; stosl":
        :"a" (BAD_PAGE + PAGE_TABLE),
        "D" ((long) empty_bad_page_table),
        "c" (PTRS_PER_PAGE));
    return (unsigned long) empty_bad_page_table;
}

unsigned long __bad_page(void) {
    extern char empty_bad_page[PAGE_SIZE];

    __asm__ __volatile__("cld; rep; stosl":
        :"a" (0),
        "D" ((long) empty_bad_page),
        "c" (PTRS_PER_PAGE));
    return (unsigned long) empty_bad_page;
}

unsigned long __zero_page(void) {
    extern char empty_zero_page[PAGE_SIZE];

    __asm__ __volatile__("cld; rep; stosl":
        :"a" (0),
        "D" ((long) empty_zero_page),
        "c" (PTRS_PER_PAGE));
    return (unsigned long) empty_zero_page;
}

/**
 * 用于将所有物理内存先映射到0x0和0xC0000000虚拟地址空间
 * head.S里已经映射了0-4M物理空间，这里从0映射所有物理内存
 * 页目录表用来存放页表，从start_mem开始分配，第一个页表已经存在
 **/
unsigned long paging_init(unsigned long start_mem, unsigned long end_mem) {
    unsigned long *pg_dir;
    unsigned long *pg_table;
    unsigned long tmp;
    unsigned long address;

    start_mem = PAGE_ALIGN(start_mem);
    address = 0;
    pg_dir = swapper_pg_dir;            // 0x2000 => [0x0000 - 0x4000)
    while (address < end_mem) {
        tmp = *(pg_dir + 768);      // at virtual addr 0xC0000000
        if (!tmp) {
            tmp = start_mem | PAGE_TABLE;
            *(pg_dir + 768) = tmp;
            // 用作页表
            start_mem += PAGE_SIZE;
        }
        *pg_dir = tmp;              // 为了初始化，同时也映射到虚拟地址的开始部分
        pg_dir++;
        // 继续映射初始化页表
        pg_table = (unsigned long *) (tmp & PAGE_MASK);
        for (tmp = 0; tmp < PTRS_PER_PAGE; tmp++, pg_table++) {
            if (address < end_mem)
                *pg_table = address | PAGE_SHARED;
            else
                *pg_table = 0;
            address += PAGE_SIZE;
        }
    }
    invalidate();
    return start_mem;
}

void mem_init(unsigned long start_low_mem, unsigned long start_mem, unsigned long end_mem) {
    int codepages = 0;
    int reservedpages = 0;
    int datapages = 0;
    unsigned long tmp;
    unsigned short *p;
    extern int etext;

    cli();
    end_mem &= PAGE_MASK;
    high_memory = end_mem;
    start_mem += 0x0000000f;
    start_mem &= ~0x0000000f;
    tmp = MAP_NR(end_mem);
    mem_map = (unsigned short *) start_mem;
    // 从start_mem开始的tmp个元素的short数组用来存放物理内存映射
    p = mem_map + tmp;
    // 开始内存后移
    start_mem = (unsigned long) p;
    // 全部先初始化为保留
    while (p > mem_map)
        *--p = MAP_PAGE_RESERVED;
    // start_low_mem 0xa0000 reserve mem_map start_mem end_mem
    start_low_mem = PAGE_ALIGN(start_low_mem);
    start_mem = PAGE_ALIGN(start_mem);
    while (start_low_mem < 0xA0000) {
        mem_map[MAP_NR(start_low_mem)] = 0;
        start_low_mem += PAGE_SIZE;
    }
    while (start_mem < end_mem) {
        mem_map[MAP_NR(start_mem)] = 0;
        start_mem += PAGE_SIZE;
    }
    // sound
    free_page_list = 0;
	nr_free_pages = 0;
    for (tmp = 0; tmp < end_mem; tmp += PAGE_SIZE) {
        if (mem_map[MAP_NR(tmp)]) {
            if (tmp >= 0xA0000 && tmp < 0x100000)
                reservedpages++;
            else if (tmp < (unsigned long) &etext)
                codepages++;
            else
                datapages++;
            continue;
        }
        // 一个page的前4个字节存放了下一个空闲页的指针
        *(unsigned long *) tmp = free_page_list;
        free_page_list = tmp;
        nr_free_pages++;
    }
    tmp = nr_free_pages << PAGE_SHIFT;
    printk("Memory: %luk/%luk available (%dk kernel code, %dk reserved, %dk data)\n",
		tmp >> 10,
		end_mem >> 10,
		codepages << (PAGE_SHIFT-10),
		reservedpages << (PAGE_SHIFT-10),
		datapages << (PAGE_SHIFT-10));
    wp_works_ok = -1;
    // 检测写保护是否正常，写将逻辑地址0改为只读，然后写，以此来测试
    pg0[0] = PAGE_READONLY;
    invalidate();
    // 这里是内核态，0地址是逻辑地址，经过段表映射到0xc0000000，这属于线性地址
    __asm__ __volatile__("movb 0, %%al; movb %%al, 0"::);
    pg0[0] = 0;
    invalidate();
    if (wp_works_ok < 0)
        wp_works_ok = 0;
    return;
}