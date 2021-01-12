#ifndef _SCHED_H
#define _SCHED_H

#define HZ 100

#define NR_TASKS		64

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <sys/param.h>
#include <sys/resource.h>

#include <signal.h>

extern void sched_init(void);
extern void trap_init(void);

extern void panic(const char * str);

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

/* 任务(进程)数据结构，或称为进程描述符 */
struct task_struct {
/* these are hardcoded - don't touch */
/* 硬编码字段 */
	long state;						/* -1 unrunnable, 0 runnable, >0 stopped */
									/* 任务运行状态 -1 不可运行，0 可运行(就绪)， >0 已停止 */
	long counter;					/* 任务运行时间计数(递减)(滴答数)，运行时间片 */
	long priority;					/* 优先级 */
	long signal;					/* 信号位图 */
	struct sigaction sigaction[32];	/* 信号执行属性结构,对应信号将要执行的操作和标志信息 */
	long blocked;					/* 进程信号屏蔽码(对应信号位图) */ /* bitmap of masked signals */
									
/* various fields */
/* 可变字段 */
	int exit_code;					/* 退出码 */
	unsigned long start_code;		/* 代码段地址 */
	unsigned long end_code;			/* 代码段长度（字节数） */
	unsigned long end_data;			/* 代码段加数据段的长度 （字节数）*/
	unsigned long brk;				/* 总长度(字节数) */
	unsigned long start_stack;		/* 堆栈段地址 */
	long pid;						/* 进程标识号(进程号) */
	long pgrp;						/* 进程组号 */
	long session;					/* 会话号 */
	long leader;					/* 会话首领 */
	int	groups[NGROUPS];			/* 进程所属组号（一个进程可属于多个组） */
	/* 
	 * pointers to parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with 
	 * p->p_pptr->pid)
	 */
	struct task_struct *p_pptr;		/* 指向父进程的指针 */
	struct task_struct *p_cptr;		/* 指向最新子进程的指针 */
	struct task_struct *p_ysptr;	/* 指向比自己后创建的相邻进程的指针 */
	struct task_struct *p_osptr;	/* 指向比自己早创建的相邻进程的指针 */
	unsigned short uid;				/* 用户id */
	unsigned short euid;			/* 有效用户id */
	unsigned short suid;			/* 保存的设置用户id */
	unsigned short gid;				/* 组id */
	unsigned short egid;			/* 有效组id */
	unsigned short sgid;			/* 保存的设置组id */
	unsigned long timeout;			/* 内核定时超时值 */
	unsigned long alarm;			/* 报警定时值(滴答数) */
	long utime;						/* 用户态运行时间(滴答数) */
	long stime;						/* 内核态运行时间(滴答数) */
	long cutime;					/* 子进程用户态运行时间 */
	long cstime;					/* 子进程内核态运行时间 */
	long start_time;				/* 进程开始运行时刻 */
	struct rlimit rlim[RLIM_NLIMITS];	/* 进程资源使用统计数组 */
	unsigned int flags;					/* per process flags, defined below */
										/* 各进程的标志 */
	unsigned short used_math;			/* 是否使用了协处理器的标志 */
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
					/* 进程使用tty终端的子设备号。-1表示没有使用 */
	unsigned short umask;			/* 文件创建属性屏蔽位 */
	struct m_inode * pwd;			/* 当前工作目录i节点结构指针 */
	struct m_inode * root;			/* 根目录i节点结构指针 */
	struct m_inode * executable;	/* 执行文件i节点结构指针 */
	struct m_inode * library;		/* 被加载库文件i节点结构指针 */
	unsigned long close_on_exec;	/* 执行时关闭文件句柄位图标志 */
	struct file * filp[NR_OPEN];	/* 文件结构指针表，最多32项。表项号即是文件描述符的值 */
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];		/* 局部描述符表, 0 - 空，1 - 代码段cs，2 - 数据和堆栈段ds&ss */
/* tss for this task */
	struct tss_struct tss;			/* 进程的任务状态段信息结构 */
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,0,0,0, \
/* suppl grps*/ {NOGROUP,}, \
/* proc links*/ &init_task.task,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* timeout */	0,0,0,0,0,0,0, \
/* rlimits */   { {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff},  \
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}, \
		  {0x7fffffff, 0x7fffffff}, {0x7fffffff, 0x7fffffff}}, \
/* flags */	0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)

#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))

#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))

#endif