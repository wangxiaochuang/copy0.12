#include <errno.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/sched.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

#define MAX_ARG_PAGES 32

int sys_uselib(const char *library) {
    struct m_inode *inode;
    unsigned long base;
    if (get_limit(0x17) != TASK_SIZE) return -EINVAL;
    if (library) {
        if (!(inode = namei(library))) {
            return -ENOENT;
        }
    } else {
        inode = NULL;
    }
    iput(current->library);
    current->library = NULL;
    base = get_base(current->ldt[2]);
    base += LIBRARY_OFFSET;
    free_page_tables(base, LIBRARY_SIZE);
    current->library = inode;
    return 0;
}

static unsigned long * create_tables(char * p, int argc, int envc) {
	unsigned long *argv, *envp;
	unsigned long * sp;

	/* 栈指针是以4字节为边界进行寻址的，故为0xfffffffc */
	// 指向环境、参数所在位置
	sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
	sp -= envc + 1;
	envp = sp;
	sp -= argc + 1;
	argv = sp;
	put_fs_long((unsigned long) envp, --sp);
	put_fs_long((unsigned long) argv, --sp);
	put_fs_long((unsigned long) argc, --sp);
	while (argc-- > 0) {
		// 只存放参数首地址
		put_fs_long((unsigned long) p, argv++);
		while (get_fs_byte(p++));
	}
	// NULL结尾
	put_fs_long(0, argv);
	while (envc-- > 0) {
		put_fs_long((unsigned long) p, envp++);
		while (get_fs_byte(p++));
	}
	put_fs_long(0, envp);
	return sp;
}

static int count(char **argv) {
	int i = 0;
	char **tmp;

	if ((tmp = argv)) {
		while (get_fs_long((unsigned long *) (tmp++))) {
			i++;
		}
	}
	return i;
}

/**
 * p: PAGE_SIZE * MAX_ARG_PAGES - 4;
 * page: 页面空间数组，最多MAX_ARG_PAGES项
 **/
static unsigned long copy_strings(int argc, char ** argv, unsigned long *page,
		unsigned long p, int from_kmem) {
    char *tmp, *pag;
	int len, offset = 0;
	unsigned long old_fs, new_fs;

	if (!p) {
		return 0;
	}
	new_fs = get_ds();
	old_fs = get_fs();
	if (from_kmem == 2) {
		set_fs(new_fs);
	}
	while (argc-- > 0) {
		if (from_kmem == 1) {
			set_fs(new_fs);
		}
		// 从最后一个参数开始取
		if (!(tmp = (char *) get_fs_long(((unsigned long *) argv) + argc))) {
			panic("argc is wrong");
		}
		if (from_kmem == 1) {
			set_fs(old_fs);
		}
		// 先计算这个参数的字符串长度
		len = 0;
		do {
			len++;
		} while (get_fs_byte(tmp++));
		if (p - len < 0) {
			set_fs(old_fs);
			return 0;
		}
		// pag为当前哪个页面，offset为页面中的偏移
		while (len) {
			--p; --tmp; --len;
			if (--offset < 0) {
				offset = p % PAGE_SIZE;
				if (from_kmem == 2) {
					set_fs(old_fs);
				}
				if (!(pag = (char *) page[p/PAGE_SIZE]) && 
					!(pag = (char *) (page[p/PAGE_SIZE] = get_free_page()))) {
					return 0;
				}
				if (from_kmem == 2) {
					set_fs(new_fs);
				}
			}
			*(pag + offset) = get_fs_byte(tmp);
		}
	}
	if (from_kmem == 2) {
		set_fs(old_fs);
	}
	return p;
}

/**
 * 修改任务的局部描述符表内容
 * 修改局部描述符表LDT中描述符的段基址和段限长，并将参数和环境空间页面放置在数据段末端。
 * @param[in]	text_size	执行文件头部中a_text字段给出的代码段长度值
 * @param[in]	page		参数和环境空间页面指针数组
 * @retval		数据段限长值(64MB)
 */
static unsigned long change_ldt(unsigned long text_size, unsigned long * page) {
	unsigned long code_limit, data_limit, code_base, data_base;
	int i;

	code_limit = TASK_SIZE;
	data_limit = TASK_SIZE;
	code_base = get_base(current->ldt[1]);
	data_base = code_base;
	set_base(current->ldt[1], code_base);
	set_limit(current->ldt[1], code_limit);
	set_base(current->ldt[2], data_base);
	set_limit(current->ldt[2], data_limit);
	/* FS段寄存器中放入局部表数据段描述符的选择符(0x17) */
	__asm__("pushl $0x17\n\tpop %%fs"::);
	data_base += data_limit - LIBRARY_SIZE;		// 库文件代码占用进程空间末端部分
	for (i = MAX_ARG_PAGES - 1; i >= 0; i--) {
		data_base -= PAGE_SIZE;
		if (page[i]) {
			put_dirty_page(page[i], data_base);
		}
	}
	return data_limit;
}
int do_execve(unsigned long * eip, long tmp, char * filename,
	char ** argv, char ** envp) {
	struct m_inode * inode;
	struct buffer_head * bh;
	struct exec ex;
	unsigned long page[MAX_ARG_PAGES]; /* 参数和环境串空间页面指针数组 */
	int i, argc, envc;
	int e_uid, e_gid;
	int retval;
	int sh_bang = 0;					/* 控制是否需要执行脚本文件 */
	unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4;

	// eip是调用本次系统调用的原用户程序代码指针
	// eip[1]是调用本次系统调用的原用户程序的代码段寄存器CS值
    if ((0xffff & eip[1]) != 0x000f) {
		panic("execve called from supervisor mode");
		char buf[128], *cp, *interp, *i_name, *i_arg;
		unsigned long old_fs;
	}
	// 参数最多32个页面
	for (i = 0; i < MAX_ARG_PAGES; i++) {
		page[i] = 0;
	}
	if (!(inode = namei(filename))) {
		return -ENOENT;
	}
	argc = count(argv);
	envc = count(envp);

restart_interp:
	if (!S_ISREG(inode->i_mode)) {
		retval = -EACCES;
		goto exec_error2;
	}

	i = inode->i_mode;
	// 如果有SUID位权限就取文件权限，否则就是当前进程权限
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
	e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

	// 查看当前进程是否有权限执行
	// 如果当前进程的有效UID就是文件的UID，那么就查属主权限
	if (current->euid == inode->i_uid) {
		i >>= 6;
	// 然后看组权限
	} else if (in_group_p(inode->i_gid)) {
		i >>= 3;
	}
	// 如果还是没有就是其他权限

	// 普通进程没有权限 并且不是超级用户，或者文件也没执行权限
	if (!(i & 1) && !((inode->i_mode & 0111) && suser())) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
	// 读取第一块数据
	if (!(bh = bread(inode->i_dev, inode->i_zone[0]))) {
		retval = -EACCES;
		goto exec_error2;
	}
	
	ex = *((struct exec *) bh->b_data);
	if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
		panic("it is script file\n");
	}
	brelse(bh);
	/* 对这个内核来说，它仅支持ZMAGIC执行文件格式，不支持含有代码或数据重定位信息的执
	行文件，执行文件实在太大或者执行文件残缺不全也不行 */
	if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
		ex.a_text + ex.a_data + ex.a_bss > 0x3000000 ||
		inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF(ex)) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (N_TXTOFF(ex) != BLOCK_SIZE) {
		printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (!sh_bang) {
		p = copy_strings(envc, envp, page, p, 0);
		p = copy_strings(argc, argv, page, p, 0);
		if (!p) {
			retval = -ENOMEM;
			goto exec_error2;
		}
	}
	if (current->executable) {
		iput(current->executable);
	}
	current->executable = inode;
	current->signal = 0;
	for (i = 0; i < 32; i++) {
		current->sigaction[i].sa_mask = 0;
		current->sigaction[i].sa_flags = 0;
		if (current->sigaction[i].sa_handler != SIG_IGN) {
			current->sigaction[i].sa_handler = NULL;
		}
	}
	for (i = 0; i < NR_OPEN; i++) {
		if ((current->close_on_exec >> i) & 1) {
			sys_close(i);
		}
	}
	current->close_on_exec = 0;
	free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]), get_limit(0x17));
	if (last_task_used_math == current) {
		last_task_used_math = NULL;
	}
	current->used_math = 0;
	p += change_ldt(ex.a_text, page);
	p -=  LIBRARY_SIZE + MAX_ARG_PAGES * PAGE_SIZE;
	p = (unsigned long) create_tables((char *)p, argc, envc);
	current->brk = ex.a_bss + 
		(current->end_data = ex.a_data + 
		(current->end_code = ex.a_text));
	current->start_stack = p & 0xfffff000;
	current->suid = current->euid = e_uid;
	current->sgid = current->egid = e_gid;
	eip[0] = ex.a_entry;
	eip[3] = p;
	return 0;

exec_error2:
	iput(inode);
exec_error1:
	for (i = 0; i < MAX_ARG_PAGES; i ++) {
		free_page(page[i]);
	}
	return(retval);
}
