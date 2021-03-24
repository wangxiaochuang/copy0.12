#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H
#define __KERNEL__

#define HZ 100

/*
 * System setup flags..
 */
extern int hard_math;
extern int x86;
extern int ignore_irq13;
extern int wp_works_ok;

extern int EISA_bus;

#include <linux/tasks.h>
#include <asm/system.h>

#define TASK_SIZE 0xc0000000

#define IO_BITMAP_SIZE	32

#define FSHIFT		11		/* nr of bits of precision */
#define FIXED_1		(1<<FSHIFT)	/* 1.0 as fixed-point */
#define LOAD_FREQ	(5*HZ)		/* 5 sec intervals */
#define EXP_1		1884		/* 1/exp(5sec/1min) as fixed-point */
#define EXP_5		2014		/* 1/exp(5sec/5min) */
#define EXP_15		2037		/* 1/exp(5sec/15min) */

#define CALC_LOAD(load,exp,n) \
	load *= exp; \
	load += n*(FIXED_1-exp); \
	load >>= FSHIFT;

#define CT_TO_SECS(x)	((x) / HZ)
#define CT_TO_USECS(x)	(((x) % HZ) * 1000000/HZ)

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/time.h>
#include <linux/param.h>
#include <linux/resource.h>
#include <linux/vm86.h>
#include <linux/math_emu.h>

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4
#define TASK_SWAPPING		5

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifdef __KERNEL__
extern void sched_init(void);
extern void show_state(void);
extern void trap_init(void);
asmlinkage void schedule(void);
#endif

struct i387_hard_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

struct i387_soft_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long    top;
	struct fpu_reg	regs[8];	/* 8*16 bytes for each FP-reg = 128 bytes */
	unsigned char	lookahead;
	struct info	*info;
	unsigned long	entry_eip;
};

union i387_union {
	struct i387_hard_struct hard;
	struct i387_soft_struct soft;
};

struct tss_struct {
	unsigned short	back_link,__blh;
	unsigned long	esp0;
	unsigned short	ss0,__ss0h;
	unsigned long	esp1;
	unsigned short	ss1,__ss1h;
	unsigned long	esp2;
	unsigned short	ss2,__ss2h;
	unsigned long	cr3;
	unsigned long	eip;
	unsigned long	eflags;
	unsigned long	eax,ecx,edx,ebx;
	unsigned long	esp;
	unsigned long	ebp;
	unsigned long	esi;
	unsigned long	edi;
	unsigned short	es, __esh;
	unsigned short	cs, __csh;
	unsigned short	ss, __ssh;
	unsigned short	ds, __dsh;
	unsigned short	fs, __fsh;
	unsigned short	gs, __gsh;
	unsigned short	ldt, __ldth;
	unsigned short	trace, bitmap;
	unsigned long	io_bitmap[IO_BITMAP_SIZE+1];
	unsigned long	tr;
	unsigned long	cr2, trap_no, error_code;
	union i387_union i387;
};

struct task_struct {
    volatile long state;
    long counter;
    long priority;
    unsigned long signal;
    unsigned long blocked;
    unsigned long flags;
    int errno;
    int debugreg[8];
    struct task_struct *next_task, *prev_task;
    struct sigaction sigaction[32];
    unsigned long saved_kernel_stack;
    unsigned long kernel_stack_page;
    int exit_code, exit_signal;
    int elf_executable:1;
	int dumpable:1;
	int swappable:1;
	int did_exec:1;
    unsigned long start_code,end_code,end_data,start_brk,brk,start_stack,start_mmap;
    unsigned long arg_start, arg_end, env_start, env_end;
    int pid, pgrp, session, leader;
    int groups[NGROUPS];

    struct task_struct *p_opptr, *p_pptr, *p_cptr, *p_ysptr, *p_osptr;
    struct wait_queue *wait_chldexit;

    unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	unsigned long timeout;
	unsigned long it_real_value, it_prof_value, it_virt_value;
	unsigned long it_real_incr, it_prof_incr, it_virt_incr;
	long utime,stime,cutime,cstime,start_time;
	unsigned long min_flt, maj_flt;
	unsigned long cmin_flt, cmaj_flt;
	struct rlimit rlim[RLIM_NLIMITS]; 
	unsigned short used_math;
	unsigned short rss;	/* number of resident pages */
	char comm[16];
	struct vm86_struct * vm86_info;
	unsigned long screen_bitmap;
/* file system info */
	int link_count;
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct inode * pwd;
	struct inode * root;
	struct inode * executable;
	struct vm_area_struct * mmap;
	struct shm_desc *shm;
	struct sem_undo *semun;
	struct file * filp[NR_OPEN];
	fd_set close_on_exec;
/* ldt for this task - used by Wine.  If NULL, default_ldt is used */
	struct desc_struct *ldt;
/* tss for this task */
	struct tss_struct tss;
#ifdef NEW_SWAP
	unsigned long old_maj_flt;	/* old value of maj_flt */
	unsigned long dec_flt;		/* page fault count of the last time */
	unsigned long swap_cnt;		/* number of pages to swap on next pass */
	short swap_table;		/* current page table */
	short swap_page;		/* current page */
#endif // NEW_SWAP
	struct vm_area_struct *stk_vma;
};

// per process flags
#define PF_ALIGNWARN	0x00000001	/* Print alignment warning msgs */
					/* Not implemented yet, only for 486*/
#define PF_PTRACED	0x00000010	/* set if ptrace (0) has been called. */
#define PF_TRACESYS	0x00000020	/* tracing system calls */

// cloning flags
#define CSIGNAL		0x000000ff	/* signal mask to be sent at exit */
#define COPYVM		0x00000100	/* set if VM copy desired (like normal fork()) */
#define COPYFD		0x00000200	/* set if fd's should be copied, not shared (NI) */

#define INIT_TASK \
/* state etc */	{ 0,15,15,0,0,0,0, \
/* debugregs */ { 0, },            \
/* schedlink */	&init_task,&init_task, \
/* signals */	{{ 0, },}, \
/* stack */	0,(unsigned long) &init_kernel_stack, \
/* ec,brk... */	0,0,0,0,0,0,0,0,0,0,0,0,0, \
/* argv.. */	0,0,0,0, \
/* pid etc.. */	0,0,0,0, \
/* suppl grps*/ {NOGROUP,}, \
/* proc links*/ &init_task,&init_task,NULL,NULL,NULL,NULL, \
/* uid etc */	0,0,0,0,0,0, \
/* timeout */	0,0,0,0,0,0,0,0,0,0,0,0, \
/* min_flt */	0,0,0,0, \
/* rlimits */   { {LONG_MAX, LONG_MAX}, {LONG_MAX, LONG_MAX},  \
		  {LONG_MAX, LONG_MAX}, {LONG_MAX, LONG_MAX},  \
		  {       0, LONG_MAX}, {LONG_MAX, LONG_MAX}}, \
/* math */	0, \
/* rss */	2, \
/* comm */	"swapper", \
/* vm86_info */	NULL, 0, \
/* fs info */	0,-1,0022,NULL,NULL,NULL,NULL, \
/* ipc */	NULL, NULL, \
/* filp */	{NULL,}, \
/* cloe */	{{ 0, }}, \
/* ldt */	NULL, \
/*tss*/	{0,0, \
	 sizeof(init_kernel_stack) + (long) &init_kernel_stack, KERNEL_DS, 0, \
	 0,0,0,0,0,0, \
	 (long) &swapper_pg_dir, \
	 0,0,0,0,0,0,0,0,0,0, \
	 USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0,USER_DS,0, \
	 _LDT(0),0, \
	 0, 0x8000, \
/* ioperm */ 	{~0, }, \
	 _TSS(0), 0, 0,0, \
/* 387 state */	{ { 0, }, } \
	} \
}

extern struct task_struct init_task;
extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern unsigned long volatile jiffies;
extern volatile struct timeval xtime;
extern int need_resched;

#define CURRENT_TIME (xtime.tv_sec)

extern void sleep_on(struct wait_queue ** p);
extern void interruptible_sleep_on(struct wait_queue ** p);
extern void wake_up(struct wait_queue ** p);
extern void wake_up_interruptible(struct wait_queue ** p);

extern int send_sig(unsigned long sig,struct task_struct * p,int priv);
extern int in_group_p(gid_t grp);

extern int request_irq(unsigned int irq,void (*handler)(int));
extern int irqaction(unsigned int irq,struct sigaction * sa);

#define FIRST_TSS_ENTRY 8
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
// 获取任务n对应的tss，因为每个任务占有两个gdt，所以是左移3位再左移1位
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
#define load_TR(n) __asm__("ltr %%ax": /* no output */ :"a" (_TSS(n)))
#define load_ldt(n) __asm__("lldt %%ax": /* no output */ :"a" (_LDT(n)))
#define store_TR(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"0" (0),"i" (FIRST_TSS_ENTRY<<3))

#define switch_to(tsk) \
__asm__("cmpl %%ecx,current\n\t" \
	"je 1f\n\t" \
	"cli\n\t" \
	"xchgl %%ecx,current\n\t" \
	"ljmp %0\n\t" \
	"sti\n\t" \
	"cmpl %%ecx,last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	: /* no output */ \
	:"m" (*(((char *)&tsk->tss.tr)-4)), \
	 "c" (tsk))

static inline void add_wait_queue(struct wait_queue ** p, struct wait_queue * wait) {
	unsigned long flags;
	save_flags(flags);
	cli();
	if (!*p) {
		wait->next = wait;
		*p = wait;
	} else {
		wait->next = (*p)->next;
		(*p)->next = wait;
	}
	restore_flags(flags);
}

static inline void remove_wait_queue(struct wait_queue ** p, struct wait_queue * wait) {
	unsigned long flags;
	struct wait_queue *tmp;
	save_flags(flags);
	cli();
	if ((*p == wait) && ((*p = wait->next) == wait)) {
		*p = NULL;
	} else {
		tmp = wait;
		while (tmp->next != wait) {
			tmp = tmp->next;
		}
		tmp->next = wait->next;
	}
	wait->next = NULL;
	restore_flags(flags);
}

#define REMOVE_LINKS(p) do { unsigned long flags; \
	save_flags(flags); cli(); \
	(p)->next_task->prev_task = (p)->prev_task; \
	(p)->prev_task->next_task = (p)->next_task; \
	restore_flags(flags); \
	if ((p)->p_osptr) \
		(p)->p_osptr->p_ysptr = (p)->p_ysptr; \
	if ((p)->p_ysptr) \
		(p)->p_ysptr->p_osptr = (p)->p_osptr; \
	else \
		(p)->p_pptr->p_cptr = (p)->p_osptr; \
} while (0)

#define SET_LINKS(p) do { unsigned long flags; \
	save_flags(flags); cli(); \
	(p)->next_task = &init_task; \
	(p)->prev_task = init_task.prev_task; \
	init_task.prev_task->next_task = (p); \
	init_task.prev_task = (p); \
	restore_flags(flags); \
	(p)->p_ysptr = NULL; \
	if (((p)->p_osptr = (p)->p_pptr->p_cptr) != NULL) \
		(p)->p_osptr->p_ysptr = p; \
	(p)->p_pptr->p_cptr = p; \
} while (0);

#define for_each_task(p) \
	for (p = &init_task ; (p = p->next_task) != &init_task ; )

extern struct desc_struct default_ldt;

#define loaddebug(register) \
		__asm__("movl %0,%%edx\n\t" \
			"movl %%edx,%%db" #register "\n\t" \
			: /* no output */ \
			:"m" (current->debugreg[register]));

#endif