#include <linux/sched.h>
#include <linux/head.h>
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
    if (!page)
        return;
    if ((page & PAGE_PRESENT) && page < high_memory) {
        pg_table = (unsigned long *) ((page & PAGE_MASK) + PAGE_PTR(address));
        page = *pg_table;
        if (!(page & PAGE_PRESENT))
            return;
        if (page & PAGE_RW)
            return;
        if (!(page & PAGE_COW)) {
            if (user_esp && tsk == current) {
                current->tss.cr2 = address;
                current->tss.error_code = error_code;
                current->tss.trap_no = 14;
                send_sig(SIGSEGV, tsk, 1);
                return;
            }
        }
        if (mem_map[MAP_NR(page)] == 1) {
            *pg_table |= PAGE_RW | PAGE_DIRTY;
            invalidate();
            return;
        }
        __do_wp_page(error_code, address, tsk, user_esp);
        return;
    }
    printk("bad page directory entry %08lx\n",page);
	*pg_table = 0;
}

void do_no_page(unsigned long error_code, unsigned long address,
	struct task_struct *tsk, unsigned long user_esp) {

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
    return 0;
}

unsigned long __bad_page(void) {
    return 0;
}

unsigned long __zero_page(void) {
    return 0;
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
    // start_low_mem 0xa0000 mem_map start_mem end_mem
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