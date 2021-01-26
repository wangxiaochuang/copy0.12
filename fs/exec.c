#include <errno.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/sched.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

#define MAX_ARG_PAGES 32

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

int do_execve(unsigned long * eip, long tmp, char * filename,
	char ** argv, char ** envp) {
	struct m_inode * inode;
	struct buffer_head * bh;
	struct exec ex;
	unsigned long page[MAX_ARG_PAGES]; /* 参数和环境串空间页面指针数组 */
	int i, argc, envc;
	int e_uid, e_gid;
	int retval;
	int sh_bang = 0;
	unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4;

	// eip是调用本次系统调用的原用户程序代码指针
	// eip[1]是调用本次系统调用的原用户程序的代码段寄存器CS值
    if ((0xffff & eip[1]) != 0x000f) {
		panic("execve called from supervisor mode");
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
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
	e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

	if (current->euid == inode->i_uid) {
		i >>= 6;
	} else if (in_group_p(inode->i_gid)) {
		i >>= 3;
	}

    panic("i am here in do_execve.........");
exec_error2:
	iput(inode);
exec_error1:
	for (i = 0; i < MAX_ARG_PAGES; i ++) {
		free_page(page[i]);
	}
	return(retval);
}