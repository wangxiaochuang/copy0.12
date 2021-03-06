#include <linux/sched.h>

extern void panic(const char *fmt, ...);

unsigned long HIGH_MEMORY = 0;

#define copy_page(from, to) 	__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

#define CHECK_LAST_NR 16
static unsigned long last_pages[CHECK_LAST_NR] = { 0, };

/* 15M / 4k from 1M begin */
unsigned char mem_map [ PAGING_PAGES ] = {0, };

void free_page(unsigned long addr) {
	if (addr < LOW_MEM) return;
	if (addr < HIGH_MEMORY) {
		/* 页面号 = (addr-LOW_MEM)/4096 */
		addr -= LOW_MEM;
		addr >>= 12;
		if (mem_map[addr]--)
			return;
		/* 执行到此处表示要释放原本已经空闲的页面，内核存在问题 */
		mem_map[addr] = 0;
	}
	printk("trying to free free page: memory probably corrupted");
}

int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long page;
	unsigned long page_dir;
	unsigned long *pg_table;
	unsigned long *dir, nr;

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
		if (!(page_dir = *dir))
			continue;
		}
		*dir = 0;
		if (!(page_dir & 1)) {
			printk("free_page_tables: bad page directory.");
			continue;
		}
		pg_table = (unsigned long *) (0xfffff000 & page_dir);
		for (nr = 0 ; nr < 1024 ; nr++, pg_table++) {
			if (!(page = *pg_table))
				continue;
			*pg_table = 0;
			if (1 & page)
				free_page(0xfffff000 & page);
			else
				swap_free(page >> 1);
		}
		free_page(0xfffff000 & page_dir);
	}
	invalidate();
	for (page = 0; page < CHECK_LAST_NR; page++)
		last_pages[page] = 0;
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
	/* 源地址的目录项指针，目标地址的目录项指针，需要复制的目录项数 */
	from_dir = (unsigned long *) ((from >> 20) & 0xffc); 	/* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to >> 20) & 0xffc);
	size = ((unsigned) (size + 0x3fffff)) >> 22;
	/* 开始页表项复制操作 */
	for( ; size-- > 0 ; from_dir++, to_dir++) {
		if (*to_dir) {
			printk("copy_page_tables: already exist, "
				"probable memory corruption\n");
		}
		if (!*from_dir)
			continue;
		if (!(1 & *from_dir)) {
			*from_dir = 0;
			continue;
		}
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page())) {
			return -1;		/* Out of memory, see freeing */
		}
		*to_dir = ((unsigned long) to_page_table) | 7;
		/* 源地址在内核空间，则仅需复制前160页对应的页表项(nr = 160)，对应640KB内存 */
		/* 进程0的from是get_baes(task[0]->ldt[2]) 也就是0 */
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
				++current->rss;
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
	// 不可写但存在，因为内核态遇到写保护也会写数据，所以要手动做一下页交换
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

void do_no_page(unsigned long error_code, unsigned long address, struct task_struct *tsk) {
	static unsigned int last_checked = 0;
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block, i;
	struct inode * inode;

	for (i = 0; i < CHECK_LAST_NR; i++)
		if ((address & 0xfffff000) == last_pages[i]) {
			current->counter = 0;
			schedule();
		}
	last_checked++;
	if (last_checked >= CHECK_LAST_NR)
		last_checked = 0;
	last_pages[last_checked] = address & 0xfffff000;
	
	// 说明异常页面位置在内核或任务0和任务1所处线性地址范围内
	if (address < TASK_SIZE) {
		printk("\n\rBAD!! KERNEL PAGE MISSING\n\r");
		do_exit(SIGSEGV);
	}

	if (address - tsk->start_code > TASK_SIZE) {
		printk("Bad things happen: nonexistent page error in do_no_page\n\r");
		do_exit(SIGSEGV);
	}
	++tsk->rss;
	// 页目录项
	page = *(unsigned long *) ((address >> 20) & 0xffc);
	if (page & 1) {
		page &= 0xfffff000;
		page += (address >> 10) & 0xffc;
		tmp = *(unsigned long *) page;
		if (tmp && !(1 & tmp)) {
			swap_in((unsigned long *) page);
			return;
		}
	} else {
		// 只要页目录项指定的页表不存在就直接申请一页新内存页
		if (page)
			printk("do_no_page: bad page directory\n");
		if (!(page = get_free_page()))
			oom();
		page |= 7;
		*(unsigned long *) ((address >> 20) & 0xffc) = page;
	}
	address &= 0xfffff000;
	// 计算出缺页的地址在进程空间中的偏移长度，方便根据偏移值判定缺页所在进程空间位置，获取i节点和块号
	// 得到进程内相对偏移地址即逻辑地址，线性地址减去进程起始地址
	// code data bss - param
	tmp = address - tsk->start_code;
	if (tmp >= LIBRARY_OFFSET) {			// 缺页在库映像文件中
		inode = tsk->library;
		block = 1 + (tmp - LIBRARY_OFFSET) / BLOCK_SIZE;
	} else if (tmp < tsk->end_data) {	// 缺页在执行映像文件中
		inode = tsk->executable;
		block = 1 + tmp / BLOCK_SIZE;
	} else {
		inode = NULL;				// 动态申请的数据或栈内存页面
		block = 0;
	}

	if (!inode) {
		++tsk->min_flt;
		if (tmp > tsk->brk && tsk == current &&
			LIBRARY_OFFSET - tmp > tsk->rlim[RLIMIT_STACK].rlim_max)
				do_exit(SIGSEGV);
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
 * 4M - 16M 可被用与分配的区域
 * - 4M 高速缓冲区，设置为USED
 */
void mem_init(long start_mem, long end_mem) {
    int i;

	swap_device = 0;
	swap_file = NULL;
    HIGH_MEMORY = end_mem;
	// 只对15M的内存进行分页管理
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

void show_mem(void) {
	int i, j, k, free = 0, total = 0;
	int shared = 0;
	unsigned long * pg_tbl;

	printk("Mem-info:\n\r");
	for (i = 0; i < PAGING_PAGES; i++) {
		if (mem_map[i] == USED) {
			continue;
		}
		total++;
		if (!mem_map[i]) {
			free++;
		} else {
			shared += mem_map[i] - 1;
		}
	}
	printk("%d free pages of %d\n\r", free, total);
	printk("%d pages shared\n\r", shared);
	k = 0;
	for (i = 4; i < 1024;) {
		if (1 & pg_dir[i]) {
			if (pg_dir[i] > HIGH_MEMORY) {
				printk("page directory[%d]: %08X\n\r", i, pg_dir[i]);
				continue;
			}
			if (pg_dir[i] > LOW_MEM) {
				free++, k++;
			}
			pg_tbl = (unsigned long *) (0xfffff000 & pg_dir[i]);
			for (j = 0; j < 1024; j++) {
				if ((pg_tbl[j] & 1) && pg_tbl[j] > LOW_MEM) {
					if (pg_tbl[j] > HIGH_MEMORY) {
						printk("page_dir[%d][%d]: %08X\n\r", i, j, pg_tbl[j]);
					} else {
						k++, free++;
					}
				}
			}
		}
		i++;
		if (!(i & 15) && k) {
			k++, free++;
			printk("Process %d: %d pages\n\r", (i >> 4) - 1, k);
			k = 0;
		}
	}	
	printk("Memory found: %d (%d)\n\r\n\r", free - shared, total);
}

/**
 * 两种情况下，转换线性地址到物理地址的过程会调用该中断
 *  1. 页目录项或页表项中的存在位P标志等于0，表示页表或包含操作数的页面不再物理内存中
 *  2. 当前执行的程序没有足够的特权访问指定页面，或用户模式代码对只读页面进行写操作等
 * 错误码只有最低3个比特可用，分别表示（U/S、W/R、P），含义及作用：
 *  位0（P）: 0表示页不存在，1表示违反页级保护权限
 *  位1（W/R）: 0表示由读操作引起，1表示由写操作引起
 *  为2（U/S）: 0表示CPU正在执行超级用户代码，1表示CPU正在执行一般用户代码
 **/
void do_page_fault(unsigned long *esp, unsigned long error_code) {
	unsigned long address;
	__asm__("movl %%cr2, %0":"=r" (address));
	if (!(error_code & 1)) {
		do_no_page(error_code, address, current);
		return;
	} else {
		do_wp_page(error_code, address);
		return;
	}
}