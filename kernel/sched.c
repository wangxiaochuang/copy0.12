#include <linux/sched.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>

#include <signal.h>

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
	int i;
	printk("\rTask-info:\n\r");
	for (i = 0; i < NR_TASKS; i++) {
		if (task[i]) {
			show_task(i, task[i]);
		}
	}
}

#define LATCH (1193180 / HZ)

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

void schedule(void) {
	int i, next, c;
	struct task_struct ** p;

	for (p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p) {
			// 如果任务设置了超时定时器，并且已经超时，那么久复位
			// 如果任务处于可中断状态就设置为就绪状态
			if ((*p)->timeout && (*p)->timeout < jiffies) {
				(*p)->timeout = 0;
				if ((*p)->state == TASK_INTERRUPTIBLE) {
					(*p)->state = TASK_RUNNING;
				}
			}
			if ((*p)->alarm && (*p)->alarm < jiffies) {
				(*p)->signal |= (1 << (SIGALRM - 1));
				(*p)->alarm = 0;
			}
			// 被阻塞的信号不管
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) && 
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
			if (!*--p) {
				continue;
			}
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c) {
				c = (*p)->counter, next = i;
			}
		}
		// c 最小为0，不会为-1，如果大于0说明找到可以切换的任务
		// 如果系统中没有一个可运行的任务，则c还是为-1，会切换到任务0
		if (c) {
			break;
		}
		/* 除任务0以外，存在处于就绪状态但时间片都为0的任务，则更新counter值，然后重新寻找，这里没有考虑进程状态 */
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
			if (*p) {
				(*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
			}
		}
	}
	switch_to(next);
}
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

static inline void __sleep_on(struct task_struct **p, int state) {
	struct task_struct *tmp;

	if (!p) {
		return;
	}
	if (current == &(init_task.task)) {
		panic("task[0] trying to sleep");
	}
	tmp = *p;
	*p = current;
	current->state = state;
repeat:	schedule();
	if (*p && *p != current) {
		printk("*p != current occur\n\r");
		(**p).state = TASK_RUNNING;
		current->state = TASK_UNINTERRUPTIBLE;
		goto repeat;
	}
	if (!*p) {
		printk("Warning: *P = NULL\n\r");
	}
	if ((*p = tmp)) {
		tmp->state = 0;
	}
}

void interruptible_sleep_on(struct task_struct **p) {
	__sleep_on(p, TASK_INTERRUPTIBLE);
}

void sleep_on(struct task_struct **p) {
	__sleep_on(p, TASK_UNINTERRUPTIBLE);
}

void wake_up(struct task_struct **p) {
	if (p && *p) {
		if ((**p).state == TASK_STOPPED) {
			printk("wake_up: TASK_STOPPED");
		}
		if ((**p).state == TASK_ZOMBIE) {
			printk("wake_up: TASK_ZOMBIE");
		}
		(**p).state = TASK_RUNNING;
	}
}

unsigned char current_DOR = 0x0C;

void do_floppy_timer(void) {}

#define TIME_REQUESTS 64

static struct timer_list {
	long jiffies;
	void (*fn) ();
	struct timer_list *next;
} timer_list[TIME_REQUESTS], *next_timer = NULL;

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
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {
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

// cpl是时钟中断发生时，正在被执行的代码段的选择子
void do_timer(long cpl) {
	static int blanked = 0;

	// if黑屏计数时间不为0或黑屏间隔时间为0都说明
	if (blankcount || !blankinterval) {
		// 已经黑屏就恢复显示
		if (blanked) {
			unblank_screen();
		}
		if (blankcount) {
			blankcount--;
		}
		blanked = 0;
	} else if (!blanked) {
		blank_screen();
		blanked = 1;
	}

	if (hd_timeout) {
		if (!--hd_timeout) {
			hd_times_out();
		}
	}
	if (beepcount) {
		if (!--beepcount) {
			sysbeepstop();
		}
	}
    if (cpl) {
        current->utime++;
    } else {
        current->stime++;
    }
	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);

			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}

	if (current_DOR & 0xf0) {
		do_floppy_timer();
	}

    if ((--current->counter) > 0) {
        return;
    }
    current->counter = 0;
    if (!cpl) {
        return;
    }
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
