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
					swap_free(*pg_table >> 1);
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
			// 页面是有的，但是不存在，说明在交换分区
			if (!(1 & this_page)) {
				if (!(new_page = get_free_page())) {
					return -1;
				}
				read_swap_page(this_page >> 1, (char *) new_page);
				*to_page_table = this_page;
				// 源页表项内容指向新申请的内存页，并设置为脏页
				*from_page_table = new_page | (PAGE_DIRTY | 7);
				continue;
			}
			this_page &= ~2;	/* 让页表项对应的内存页面只读 */
			*to_page_table = this_page;
			/*
			 * 物理页面的地址在1MB以上，则需在mem_map[]中增加对应页面的引用次数
			 * 如果是内核区的代码及数据段，其读写标志不变，还是可读写，新页面设置为只读
			 * 新进程的mem_map标志也只是1，写保护异常函数里会直接改成可写
			 **/
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

unsigned long put_dirty_page(unsigned long page, unsigned long address) {
	unsigned long tmp, *page_table;

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);
	page_table = (unsigned long *) ((address >> 20) & 0xffc);
	if ((*page_table) & 1)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp = get_free_page()))
			return 0;
		*page_table = tmp | 7;
		page_table = (unsigned long *) tmp;
	}
	page_table[(address >> 12) & 0x3ff] = page | (PAGE_DIRTY | 7);
	return page;
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
	*table_entry = new_page | 7;
	invalidate();
}

void do_wp_page(unsigned long error_code, unsigned long address) {
	if (address < TASK_SIZE)
		printk("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");
	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: page error in do_wp_page\n\r");
		do_exit(SIGSEGV);
	}
	un_wp_page((unsigned long *)
		(((address >> 10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address >> 20) & 0xffc)))));
}

void write_verify(unsigned long address) {
	unsigned long page;

	// 页目录项都不存在，就不管了，其没有共享和写时复制可言
	if (!((page = *((unsigned long *) ((address >> 20) & 0xffc))) & 1)) {
		return;
	}

	page &= 0xfffff000;
	// 得到页表项的物理地址
	page += ((address >> 10) & 0xffc);
	// 不可写但存在
	if ((3 & *(unsigned long *) page) == 1) {
		un_wp_page((unsigned long *) page);
	}
	return;
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

static int try_to_share(unsigned long address, struct task_struct * p) {
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	// 目录项偏移加目录项
	from_page = to_page = ((address >> 20) & 0xffc);
	from_page += ((p->start_code >> 20) & 0xffc);
	to_page += ((current->start_code >> 20) & 0xffc);
	from = *(unsigned long *) from_page;
	if (!(from & 1))
		return 0;
	from &= 0xfffff000;
	from_page = from + ((address >> 10) & 0xffc);
	phys_addr = *(unsigned long *) from_page;

	// 物理页面干净且存在
	if ((phys_addr & 0x41) != 0x01)
		return 0;
	phys_addr &= 0xfffff000;			/* 物理页面地址 */
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
	// 当前进程目录项内容
	to = *(unsigned long *) to_page;
	if (!(to & 1)) {
		if ((to = get_free_page())) {
			*(unsigned long *) to_page = to | 7;
		} else {
			oom();
		}
	}
	to &= 0xfffff000;
	to_page = to + ((address >> 10) & 0xffc);
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
	// 共享保护
	*(unsigned long *) from_page &= ~2;
	*(unsigned long *) to_page = *(unsigned long *) from_page;

	invalidate();
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;
	mem_map[phys_addr]++;
	return 1;
}
static int share_page(struct m_inode * inode, unsigned long address) {
	struct task_struct **p;
	// 如果i节点引用计数值等于1或i节点指针为空，表示当前系统中只有1个进程在运行该执行文件
	if (inode->i_count < 2 || !inode)
		return 0;
	for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
		if (!*p)
			continue;
		if (current == *p)
			continue;
		if (address < LIBRARY_OFFSET) {
			if (inode != (*p)->executable)
				continue;
		} else {
			if (inode != (*p)->library)
				continue;
		}
		if (try_to_share(address, *p))
			return 1;
	}
	return 0;
}


void do_no_page(unsigned long error_code, unsigned long address) {
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;
	struct m_inode * inode;
	
	if (address < TASK_SIZE)
		printk("\n\rBAD!! KERNEL PAGE MISSING\n\r");

	if (address - current->start_code > TASK_SIZE) {
		printk("Bad things happen: nonexistent page error in do_no_page\n\r");
		do_exit(SIGSEGV);
	}
	page = *(unsigned long *) ((address >> 20) & 0xffc);
	if (page & 1) {
		page &= 0xfffff000;
		page += (address >> 10) & 0xffc;
		tmp = *(unsigned long *) page;
		if (tmp && !(1 & tmp)) {
			swap_in((unsigned long *) page);
			return;
		}
	}
	address &= 0xfffff000;
	// 计算出缺页的地址在进程空间中的偏移长度，方便根据偏移值判定缺页所在进程空间位置，获取i节点和块号
	tmp = address - current->start_code;
	if (tmp >= LIBRARY_OFFSET) {			// 缺页在库映像文件中
		inode = current->library;
		block = 1 + (tmp - LIBRARY_OFFSET) / BLOCK_SIZE;
	} else if (tmp < current->end_data) {	// 缺页在执行映像文件中
		inode = current->executable;
		block = 1 + tmp / BLOCK_SIZE;
	} else {
		inode = NULL;
		block = 0;
	}

	if (!inode) {
		get_empty_page(address);
		return;
	}

	// 共享成功直接返回
	if (share_page(inode, tmp))
		return;

	if (!(page = get_free_page()))
		oom();
	for (i = 0; i < 4; block++, i++)
		nr[i] = bmap(inode, block);
	bread_page(page, inode->i_dev, nr);

	// 读取执行程序最后一页，超出end_data部分清零，若离执行程序末端超过1页，说明从库文件读取，因此不清零
	i = tmp + 4096 - current->end_data;
	if (i > 4095)
		i = 0;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	if (put_page(page, address))
		return;
	free_page(page);
	oom();
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
