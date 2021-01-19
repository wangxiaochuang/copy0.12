#include <linux/sched.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>

#include <signal.h>

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
			if ((*p)->timeout && (*p)->timeout < jiffies) {
				(*p)->timeout = 0;
				if ((*p)->state == TASK_INTERRUPTIBLE) {
					(*p)->state = TASK_RUNNING;
				}
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
		/* c = -1，没有可以运行的任务（此时next=0，会切去任务0）；c > 0，找到了可以切换的任务 */
		if (c) {
			break;
		}
		/* 除任务0以外，存在处于就绪状态但时间片都为0的任务，则更新counter值，然后重新寻找 */
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

extern int printk(const char * fmt, ...);

void do_timer(long cpl) {
	if (hd_timeout) {
		if (!--hd_timeout) {
			hd_times_out();
		}
	}
    if (cpl) {
        current->utime++;
    } else {
        current->stime++;
    }
    if ((--current->counter)>0) {
        return;
    }
    current->counter = 0;
    if (!cpl) {
        return;
    }
    schedule();
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
