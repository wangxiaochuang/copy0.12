#include <linux/sched.h>
#include <linux/head.h>
#include <linux/ptrace.h>

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long error_code) {

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