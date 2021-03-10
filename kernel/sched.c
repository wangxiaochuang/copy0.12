#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>

#include <signal.h>
#include <errno.h>

/* 取信号nr在信号位图中对应位的二进制数值（信号编号1-32） */
#define _S(nr) 		(1 << ((nr)-1))

/* 除了SIGKILL和SIGSTOP信号以外其他信号都是可阻塞的 */
#define _BLOCKABLE 	(~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr, struct task_struct * p) {
	int i, j = 4096 - sizeof(struct task_struct);
	printk("%d: pid=%d, state=%d, father=%d, child=%d, ", nr, p->pid,
		p->state, p->p_pptr->pid, p->p_cptr ? p->p_cptr->pid : -1);
	i = 0;
	while (i < j && !((char *) (p + 1))[i]) {
		i++;
	}
	printk("%d/%d chars free in kstack\n\r", i, j);
	printk("   PC=%08X.", *(1019 + (unsigned long *) p));
	if (p->p_ysptr || p->p_osptr) {
		printk("   Younger sib=%d, older sib=%d\n\r", 
			p->p_ysptr ? p->p_ysptr->pid : -1,
			p->p_osptr ? p->p_osptr->pid : -1);
	} else {
		printk("\n\r");
	}
}
void show_state(void) {
	static int lock = 0;
	int i;

	cli();
	if (lock) {
		sti();
		return;
	}
	lock = 1;
	sti();
	printk("\rTask-info:\n\r");
	for (i = 0; i < NR_TASKS; i++) {
		if (task[i]) {
			show_task(i, task[i]);
		}
	}
	lock = 0;
}

#define LATCH (1193180 / HZ)

extern void mem_use(void);
extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK, };

unsigned long volatile jiffies = 0;
unsigned long startup_time = 0;
int jiffies_offset = 0;

struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct *task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE >> 2];

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

void math_state_restore() {
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_use_math = current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math = 1;
	}
}

void schedule(void) {
	int i, next, c;
	struct task_struct ** p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p) {
			// 如果任务设置了超时定时器，并且已经超时，那么就复位
			// 如果任务处于可中断状态就设置为就绪状态
			if ((*p)->timeout && (*p)->timeout < jiffies) {
				if ((*p)->state == TASK_INTERRUPTIBLE) {
					(*p)->timeout = 0;
					(*p)->state = TASK_RUNNING;
				}
			}
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1 << (SIGALRM - 1));
				(*p)->alarm = 0;
			}
			// 除了阻塞的信号外有其他信号出现，且是可中断的，那么就设置成就绪态
			if (((*p)->signal & ~(*p)->blocked) && 
				(*p)->state == TASK_INTERRUPTIBLE) {
				(*p)->state = TASK_RUNNING;
			}
		}

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		/* 找到就绪状态下时间片最大的任务，用next指向该任务 */
		while (--i) {
			if (!*--p) continue;
			// 就绪态进程中找到时间片最大的进程
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c) {
				c = (*p)->counter, next = i;
			}
		}
		// c 最小为0，不会为-1，如果大于0说明找到可以切换的任务
		// 如果系统中没有一个可运行的任务，则c还是为-1，会切换到任务0
		if (c) break;
		/* 除任务0以外，存在处于就绪状态但时间片都为0的任务，则更新counter值，然后重新寻找，这里没有考虑进程状态 */
		for (p = &LAST_TASK; p > &FIRST_TASK; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
	}
	switch_to(next);
}
int sys_pause(void)
{
	unsigned long old_blocked;
	unsigned long mask;
	struct sigaction *sa = current->sigaction;

	old_blocked = current->blocked;
	// 遍历sigaction的每一个信号处理函数，如果是忽略就屏蔽
	for (mask = 1; mask; sa++, mask += mask)
		if (sa->sa_handler == SIG_IGN)
			current->blocked |= mask;
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	current->blocked = old_blocked;
	return -EINTR;
}

void wake_up(struct task_struct **p) {
	struct task_struct *wakeup_ptr, *tmp;

	if (p && *p) {
		wakeup_ptr = *p;
		*p = NULL;
		while (wakeup_ptr && wakeup_ptr != task[0]) {
			if (wakeup_ptr->state == TASK_STOPPED)
				printk("wake_up: TASK_STOPPED\n");
			else if (wakeup_ptr->state == TASK_ZOMBIE)
				printk("wake_up: TASK_ZOMBIE\n");
			else
				wakeup_ptr->state = TASK_RUNNING;
			tmp = wakeup_ptr->next_wait;
			wakeup_ptr->next_wait = task[0];
			wakeup_ptr = tmp;
		}
	}
}

static inline void __sleep_on(struct task_struct **p, int state) {
	unsigned int flags;

	if (!p) {
		return;
	}
	if (current == task[0]) {
		panic("task[0] trying to sleep");
	}
	__asm__("pushfl; popl %0":"=r" (flags));
	current->next_wait = *p;
	task[0]->next_wait = NULL;
	*p = current;
	current->state = state;
	sti();
	schedule();
	if (current->next_wait != task[0])
		wake_up(p);
	current->next_wait = NULL;
	__asm__("pushl %0; popfl"::"r"(flags));
}

void interruptible_sleep_on(struct task_struct **p) {
	__sleep_on(p, TASK_INTERRUPTIBLE);
}

void sleep_on(struct task_struct **p) {
	__sleep_on(p, TASK_UNINTERRUPTIBLE);
}

static struct task_struct *wait_motor[4] = { NULL, NULL, NULL, NULL};
static int mon_timer[4] = {0, 0, 0, 0};
static int moff_timer[4] = {0, 0, 0, 0};
unsigned char current_DOR = 0x0C;

void do_floppy_timer(void) {}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn) ();
	struct timer_list *next;
} timer_list[TIME_REQUESTS] = { { 0, NULL, NULL}, }; 

static struct timer_list *next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void)) {
	struct timer_list *p;

	if (!fn) {
		return;
	}
	cli();
	if (jiffies <= 0) {
		(fn)();
	} else {
		for (p = timer_list; p < timer_list + TIME_REQUESTS; p++) {
			if (!p->fn) {
				break;
			}
		}
		if (p >= timer_list + TIME_REQUESTS) {
			panic("No more time requests free");
		}
		p->fn = fn;
		p->jiffies = jiffies;
		// next_timer指向最新的定时器
		p->next = next_timer;
		next_timer = p;
		// 把当前加入的这个定时器放到正确位置，时间从小到大
		while (p->next && p->next->jiffies < p->jiffies) {
			// 发现后面的定时器时间小于当前这个，那么交换其内容
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

unsigned long timer_active = 0;
struct timer_struct timer_table[32];

// cpl是时钟中断发生时，正在被执行的代码段的选择子
void do_timer(long cpl) {
	unsigned long mask;
	struct timer_struct *tp = timer_table + 0;

	// 每一位表示一个时间注册者
	for (mask = 1; mask; tp++, mask += mask) {
		if (mask > timer_active)
			break;
		if (!(mask & timer_active))
			continue;
		if (tp->expires > jiffies)
			continue;
		// 执行完了复位
		timer_active &= ~mask;
		tp->fn();
	}
	if (cpl)
		current->utime++;
	else
		current->stime++;
	if (next_timer) {
		next_timer->jiffies--;
		// 遍历所有过期的定时器，并执行
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn) (void);
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	// 当前进程还有时间
	if ((--current->counter) > 0) return;
	current->counter = 0;
	// 内核线程不能调度出去
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds) {
	int old = current->alarm;

	if (old) {
		old = (old - jiffies) / HZ;
	}
	current->alarm = (seconds > 0) ? (jiffies + HZ * seconds) : 0;
	return (old);
}

/* 取进程号pid */
int sys_getpid(void) {
	return current->pid;
}

/* 取父进程号ppid */
int sys_getppid(void) {
	return current->p_pptr->pid;
}

/* 取用户id */
int sys_getuid(void) {
	return current->uid;
}

/* 取有效用户id */
int sys_geteuid(void) {
	return current->euid;
}

/* 取组号gid */
int sys_getgid(void) {
	return current->gid;
}

/* 取有效的组号egid */
int sys_getegid(void) {
	return current->egid;
}

int sys_nice(long increment) {
	if (increment < 0 && !suser())
		return -EPERM;
	if (increment > current->priority)
		increment = current->priority - 1;
	current->priority -= increment;
	return 0;
}

void sched_init(void) {
    int i;
    struct desc_struct *p;

    if (sizeof(struct sigaction) != 16) {
		panic("Struct sigaction MUST be 16 bytes");
	}

    set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));
    p = gdt + 2 + FIRST_TSS_ENTRY;
    for (i = 1; i < NR_TASKS; i++) {
        task[i] = NULL;
        p->a = p->b = 0;
        p++;
        p->a = p->b = 0;
        p++;
    }
    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl");
    ltr(0);
    lldt(0);

    outb_p(0x36, 0x43);				/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff, 0x40);	/* LSB */
	outb(LATCH >> 8, 0x40);		/* MSB */

    set_intr_gate(0x20, &timer_interrupt);
	outb(inb_p(0x21)&~0x01, 0x21);

    set_system_gate(0x80, &system_call);
}
