#include <linux/sched.h>

extern void panic(const char *fmt, ...);

unsigned long HIGH_MEMORY = 0;

#define copy_page(from, to) 	__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

/* 15M / 4k from 1M begin */
unsigned char mem_map [ PAGING_PAGES ] = {0, };

void free_page(unsigned long addr) {
	if (addr < LOW_MEM) {
		return;
	}
	if (addr >= HIGH_MEMORY) {
		panic("trying to free nonexistent page");
	}
	/* 页面号 = (addr-LOW_MEM)/4096 */
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--) {
		return;
	}
	/* 执行到此处表示要释放原本已经空闲的页面，内核存在问题 */
	mem_map[addr] = 0;
	panic("trying to free free page");
}

int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	if (from & 0x3fffff) {	/* 参数from给出的线性基地址是否在4MB的边界处 */
		panic("free_page_tables called with wrong alignment");
	}
	if (!from) {			/* from=0说明试图释放内核和缓冲所在的物理内存空间 */
		panic("Trying to free up swapper memory space");
	}
	/* 计算size指定长度所占的页目录数（4MB的进位整数倍，向上取整），例如size=4.01MB则size=2 */
	size = (size + 0x3fffff) >> 22;
	/* 页目录项指针 */
	dir = (unsigned long *) ((from >> 20) & 0xffc); 			/* _pg_dir = 0 */
	/* 遍历需要释放的页目录项，释放对应页表中的页表项 */
	for ( ; size-- > 0 ; dir++) {
		if (!(1 & *dir)) {
			continue;
		}
		pg_table = (unsigned long *) (0xfffff000 & *dir);
		for (nr = 0 ; nr < 1024 ; nr++) {
			if (*pg_table) {
				if (1 & *pg_table) {	/* 在物理内存中  */
					free_page(0xfffff000 & *pg_table);
				} else {				/* 在交换设备中 */
					// swap_free(*pg_table >> 1);
                    panic("in swap");
				}
				*pg_table = 0;
			}
			pg_table++;
		}
		free_page(0xfffff000 & *dir);
		*dir = 0;
	}
	invalidate();
	return 0;
}

int copy_page_tables(unsigned long from, unsigned long to, long size) {
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long new_page;
	unsigned long nr;

	/* 源地址和目的地址都需要在4MB内存边界地址 */
	if ((from & 0x3fffff) || (to & 0x3fffff)) {
		panic("copy_page_tables called with wrong alignment");
	}
	/* 源地址的目录项指针，目标地址的目录项指针， 需要复制的目录项数 */
	from_dir = (unsigned long *) ((from >> 20) & 0xffc); 	/* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to >> 20) & 0xffc);
	size = ((unsigned) (size + 0x3fffff)) >> 22;
	/* 开始页表项复制操作 */
	for( ; size-- > 0 ; from_dir++, to_dir++) {
		if (1 & *to_dir) {
			panic("copy_page_tables: already exist");
		}
		if (!(1 & *from_dir)) {
			continue;
		}
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page())) {
			return -1;		/* Out of memory, see freeing */
		}
		*to_dir = ((unsigned long) to_page_table) | 7;
		/* 源地址在内核空间，则仅需复制前160页对应的页表项(nr = 160)，对应640KB内存 */
		nr = (from == 0) ? 0xA0 : 1024;
		/* 循环复制当前页表的nr个内存页面表项 */
		for ( ; nr-- > 0 ; from_page_table++, to_page_table++) {
			this_page = *from_page_table;
			if (!this_page) {
				continue;
			}
			/* 该页面在交换设备中，申请一页新的内存，然后将交换设备中的数据读取到该页面中 */
			if (!(1 & this_page)) {
                panic("in swap");
			}
			this_page &= ~2;	/* 让页表项对应的内存页面只读 */
			*to_page_table = this_page;
			/* 物理页面的地址在1MB以上，则需在mem_map[]中增加对应页面的引用次数 */
			if (this_page > LOW_MEM) {
				*from_page_table = this_page;	/* 令源页表项也只读 */
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
        }
    }
	invalidate();
	return 0;
}

static unsigned long put_page(unsigned long page, unsigned long address)
{
	unsigned long tmp, *page_table;

	/* NOTE !!! This uses the fact that _pg_dir=0 */
	/* 注意!!! 这里使用了页目录表基地址pg_dir=0的条件 */

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	/* page指向的页面未标记为已使用，故不能做映射 */
	if (mem_map[(page - LOW_MEM) >> 12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);

	/* 根据address从页目录表取出页表地址 */
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)	/* 页表存在 */
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7; 	/* 置位3个标志(U/S，W/R，P) */
		page_table = (unsigned long *) tmp;
	}
	/* 在页表中设置页面地址，并置位3个标志(U/S，W/R，P) */
	page_table[(address >> 12) & 0x3ff] = page | 7;

	/* no need for invalidate */
	return page;
}

void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	/* 若不能取得一空闲页面，或者不能将所取页面放置到指定地址处，则显示内存不够的信息 */
	if (!(tmp = get_free_page()) || !put_page(tmp, address)) {
		free_page(tmp);		/* 0 is ok - ignored */
		oom();
	}
}

void do_no_page(unsigned long error_code, unsigned long address) {
	if (address < TASK_SIZE)
		printk("\n\rBAD!! KERNEL PAGE MISSING\n\r");

	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: nonexistent page error in do_no_page\n\r");
		// do_exit(SIGSEGV);
	}
	address &= 0xfffff000;
	get_empty_page(address);
	return;
}

void un_wp_page(unsigned long * table_entry) {
	unsigned long old_page, new_page;

	old_page = 0xfffff000 & *table_entry;

	/* 即如果该内存页面此时只被一个进程使用，就直接把属性改为可写即可 */
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	if (!(new_page = get_free_page()))
		oom();							/* 内存不够处理 */
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	copy_page(old_page, new_page);
	// panic("do_wp_page: 0x%08x\n", address);
	*table_entry = new_page | 7;
	invalidate();
}

void do_wp_page(unsigned long error_code, unsigned long address) {
	if (address < TASK_SIZE)
		printk("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");
	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: page error in do_wp_page\n\r");
		// do_exit(SIGSEGV);
		panic("error in do_wp_page");
	}
	un_wp_page((unsigned long *)
		(((address >> 10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address >> 20) & 0xffc)))));
}

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
    end_mem -= start_mem;
    end_mem >>= 12;
    while (end_mem-- > 0) {
        mem_map[i++] = 0;
    }
}
