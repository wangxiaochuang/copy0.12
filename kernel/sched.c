#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/sys.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define TIMER_IRQ 0

#include <linux/timex.h>

// 一个tick也就是一个时钟中断有多少微秒
long tick = 1000000 / HZ;               /* timer interrupt period */
volatile struct timeval xtime;          // the current time
int tickadj = 500/HZ;                   // microsecs

int time_status = TIME_BAD;     /* clock synchronization status */
long time_offset = 0;           /* time adjustment (us) */
long time_constant = 0;         /* pll time constant */
long time_tolerance = MAXFREQ;  /* frequency tolerance (ppm) */
long time_precision = 1; 	/* clock precision (us) */
long time_maxerror = 0x70000000;/* maximum error */
long time_esterror = 0x70000000;/* estimated error */
long time_phase = 0;            /* phase offset (scaled us) */
long time_freq = 0;             /* frequency offset (scaled ppm) */
long time_adj = 0;              /* tick adjust (scaled 1 / HZ) */
long time_reftime = 0;          /* time at last adjustment (s) */

long time_adjust = 0;
long time_adjust_step = 0;

int need_resched = 0;

int hard_math = 0;
int x86 = 0;
int ignore_irq13 = 0;
int wp_works_ok = 0;

int EISA_bus = 0;

#define _S(nr) (1<<((nr)-1))

asmlinkage int system_call(void);

static unsigned long init_kernel_stack[1024];
struct task_struct init_task = INIT_TASK;

unsigned long volatile jiffies=0;

struct task_struct *current = &init_task;
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&init_task, };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
    long *a;
    short b;
} stack_start = { & user_stack[PAGE_SIZE>>2], KERNEL_DS};

struct kernel_stat kstat =
	{ 0, 0, 0, { 0, 0, 0, 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#ifdef __cplusplus
extern "C" {
#endif

int sys_ni_syscall(void) {
	return -EINVAL;
}

fn_ptr sys_call_table[] = { sys_setup, sys_ni_syscall, sys_fork, sys_ni_syscall, sys_ni_syscall, sys_open, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall, sys_ni_syscall,
    sys_ni_syscall, sys_ni_syscall, sys_idle
};

int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);

#ifdef __cplusplus
}
#endif

asmlinkage void math_state_restore(void) {}

#ifndef CONFIG_MATH_EMULATION
asmlinkage void math_emulate(long arg) {}
#endif

static struct timer_list * next_timer = NULL;

unsigned long timer_active = 0;
struct timer_struct timer_table[32];

unsigned long itimer_ticks = 0;
unsigned long itimer_next = ~0;
static unsigned long lost_ticks = 0;

asmlinkage void schedule(void) {
    int c;
    struct task_struct *p;
    struct task_struct *next;
    unsigned long ticks;

    cli();
    ticks = itimer_ticks;
    itimer_ticks = 0;
    itimer_next = ~0;
    sti();
    need_resched = 0;
    p = &init_task;
    for (;;) {
        if ((p = p->next_task) == &init_task)
            goto confuse_gcc1;
        if (ticks && p->it_real_value) {
            if (p->it_real_value <= ticks) {
                send_sig(SIGALRM, p, 1);
                if (!p->it_real_incr) {
                    p->it_real_value = 0;
                    goto end_itimer;
                }
                do {
                    p->it_real_value += p->it_real_incr;
                } while (p->it_real_value <= ticks);
            }
            p->it_real_value -= ticks;
            if (p->it_real_value < itimer_next)
                itimer_next = p->it_real_value;
        }
end_itimer:
        if (p->state != TASK_INTERRUPTIBLE)
            continue;
        if (p->signal & ~p->blocked) {
            p->state = TASK_RUNNING;
            continue;
        }
        if (p->timeout && p->timeout <= jiffies) {
            p->timeout = 0;
            p->state = TASK_RUNNING;
        }
    }

confuse_gcc1:
    c = -1;
    next = p = &init_task;
    for (;;) {
        if ((p = p->next_task) == &init_task)
            goto confuse_gcc2;
        if (p->state == TASK_RUNNING && p->counter > c)
            c = p->counter, next = p;
    } 
confuse_gcc2:
    if (!c) {
        for_each_task(p)
            p->counter = (p->counter >> 1) + p->priority;
    }
    if (current != next)
        kstat.context_swtch++;
    
    switch_to(next);
    if (current->debugreg[7]) {
        loaddebug(0);
		loaddebug(1);
		loaddebug(2);
		loaddebug(3);
		loaddebug(6);
    }
}

void wake_up(struct wait_queue **q) {
    struct wait_queue *tmp;
    struct task_struct *p;

    if (!q || !(tmp = *q))
        return;
    do {
        if ((p = tmp->task) != NULL) {
            if ((p->state == TASK_UNINTERRUPTIBLE) || (p->state == TASK_INTERRUPTIBLE)) {
                p->state = TASK_RUNNING;
                if (p->counter > current->counter)
                    need_resched = 1;
            }
        }
        if (!tmp->next) {
            printk("wait_queue is bad (eip = %08lx)\n",((unsigned long *) q)[-1]);
			printk("        q = %p\n",q);
			printk("       *q = %p\n",*q);
			printk("      tmp = %p\n",tmp);
			break;
        }
        tmp = tmp->next;
    } while (tmp != *q);
}

void wake_up_interruptible(struct wait_queue **q) {
    struct wait_queue *tmp;
	struct task_struct * p;

    if (!q || !(tmp = *q))
        return;
    do {
        if ((p = tmp->task) != NULL) {
            if (p->state == TASK_INTERRUPTIBLE) {
				p->state = TASK_RUNNING;
				if (p->counter > current->counter)
					need_resched = 1;
			}
        }
        if (!tmp->next) {
			printk("wait_queue is bad (eip = %08lx)\n",((unsigned long *) q)[-1]);
			printk("        q = %p\n",q);
			printk("       *q = %p\n",*q);
			printk("      tmp = %p\n",tmp);
			break;
		}
        tmp = tmp->next;
    } while (tmp != *q);
}

static inline void __sleep_on(struct wait_queue **p, int state) {
    unsigned long flags;
    struct wait_queue wait = { current, NULL };

    if (!p)
        return;
    if (current == task[0])
        panic("task[0] trying to sleep");
    current->state = state;
    add_wait_queue(p, &wait);
    save_flags(flags);
    sti();
    schedule();
    remove_wait_queue(p, &wait);
    restore_flags(flags);
}

void interruptible_sleep_on(struct wait_queue **p) {
    __sleep_on(p,TASK_INTERRUPTIBLE);
}

void sleep_on(struct wait_queue **p) {
	__sleep_on(p,TASK_UNINTERRUPTIBLE);
}

unsigned long avenrun[3] = { 0,0,0 };

/*
 * Nr of active tasks - counted in fixed-point numbers
 */
static unsigned long count_active_tasks(void) {
	struct task_struct **p;
	unsigned long nr = 0;

	for(p = &LAST_TASK; p > &FIRST_TASK; --p)
		if (*p && ((*p)->state == TASK_RUNNING ||
			   (*p)->state == TASK_UNINTERRUPTIBLE ||
			   (*p)->state == TASK_SWAPPING))
			nr += FIXED_1;
	return nr;
}

static inline void calc_load(void) {
	unsigned long active_tasks; /* fixed-point */
	static int count = LOAD_FREQ;

	if (count-- > 0)
		return;
	count = LOAD_FREQ;
	active_tasks = count_active_tasks();
	CALC_LOAD(avenrun[0], EXP_1, active_tasks);
	CALC_LOAD(avenrun[1], EXP_5, active_tasks);
	CALC_LOAD(avenrun[2], EXP_15, active_tasks);
}

static void second_overflow(void) {
	long ltemp;
	/* last time the cmos clock got updated */
	static long last_rtc_update=0;
	extern int set_rtc_mmss(unsigned long);

	/* Bump the maxerror field */
	time_maxerror = (0x70000000-time_maxerror < time_tolerance) ?
	  0x70000000 : (time_maxerror + time_tolerance);

	/* Run the PLL */
	if (time_offset < 0) {
		ltemp = (-(time_offset+1) >> (SHIFT_KG + time_constant)) + 1;
		time_adj = ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		time_offset += (time_adj * HZ) >> (SHIFT_SCALE - SHIFT_UPDATE);
		time_adj = - time_adj;
	} else if (time_offset > 0) {
		ltemp = ((time_offset-1) >> (SHIFT_KG + time_constant)) + 1;
		time_adj = ltemp << (SHIFT_SCALE - SHIFT_HZ - SHIFT_UPDATE);
		time_offset -= (time_adj * HZ) >> (SHIFT_SCALE - SHIFT_UPDATE);
	} else {
		time_adj = 0;
	}

	time_adj += (time_freq >> (SHIFT_KF + SHIFT_HZ - SHIFT_SCALE))
	    + FINETUNE;

	/* Handle the leap second stuff */
	switch (time_status) {
		case TIME_INS:
		/* ugly divide should be replaced */
		if (xtime.tv_sec % 86400 == 0) {
			xtime.tv_sec--; /* !! */
			time_status = TIME_OOP;
			printk("Clock: inserting leap second 23:59:60 GMT\n");
		}
		break;

		case TIME_DEL:
		/* ugly divide should be replaced */
		if (xtime.tv_sec % 86400 == 86399) {
			xtime.tv_sec++;
			time_status = TIME_OK;
			printk("Clock: deleting leap second 23:59:59 GMT\n");
		}
		break;

		case TIME_OOP:
		time_status = TIME_OK;
		break;
	}
	if (xtime.tv_sec > last_rtc_update + 660)
	    if (set_rtc_mmss(xtime.tv_sec) == 0)
	        last_rtc_update = xtime.tv_sec;
}

static void timer_bh(void * unused) {
    unsigned long mask;
    struct timer_struct *tp;

    cli();
    while (next_timer && next_timer->expires == 0) {
        void (*fn) (unsigned long) = next_timer->function;
        unsigned long data = next_timer->data;
        next_timer = next_timer->next;
        sti();
        fn(data);
        cli();
    }
    sti();

    for (mask = 1, tp = timer_table + 0; mask; tp++, mask += mask) {
        if (mask > timer_active)
            break;
        if (!(mask & timer_active))
            continue;
        if (tp->expires > jiffies)
            continue;
        timer_active &= ~mask;
        tp->fn();
        sti();
    }
}

static void do_timer(struct pt_regs * regs) {
    unsigned long mask;
    struct timer_struct *tp;

    long ltemp;
    time_phase += time_adj;
    if (time_phase < -FINEUSEC) {
        ltemp = -time_phase >> SHIFT_SCALE;
        time_phase += ltemp << SHIFT_SCALE;
        xtime.tv_usec += tick + time_adjust_step - ltemp;
    } else if (time_phase > FINEUSEC) {
        ltemp = time_phase >> SHIFT_SCALE;
		time_phase -= ltemp << SHIFT_SCALE;
		xtime.tv_usec += tick + time_adjust_step + ltemp;
    } else {
        // 每一个时钟周期是10000微秒，累计起来
        // CURRENT_TIME取得是xtime的秒数
        xtime.tv_usec += tick + time_adjust_step;
    }
    if (time_adjust) {
        if (time_adjust > tickadj)
	        time_adjust_step = tickadj;
	    else if (time_adjust < -tickadj)
	        time_adjust_step = -tickadj;
	    else
	        time_adjust_step = time_adjust;
        /* Reduce by this step the amount of time left  */
	    time_adjust -= time_adjust_step;
    } else {
        time_adjust_step = 0;
    }

    if (xtime.tv_usec >= 1000000) {
	    xtime.tv_usec -= 1000000;
	    xtime.tv_sec++;
	    second_overflow();
	}

    jiffies++;
    calc_load();
    if ((VM_MASK & regs->eflags) || (3 & regs->cs)) {
        current->utime++;
        if (current != task[0]) {
            if (current->priority < 15)
                kstat.cpu_nice++;
            else
                kstat.cpu_user++;
        }
        if (current->it_virt_value && !(--current->it_virt_value)) {
            current->it_virt_value = current->it_virt_incr;
            send_sig(SIGVTALRM, current, 1);
        }
    } else {
        current->stime++;
        if (current != task[0])
            kstat.cpu_system++;
#ifdef CONFIG_PROFILE
		if (prof_buffer && current != task[0]) {
			unsigned long eip = regs->eip;
			eip >>= 2;
			if (eip < prof_len)
				prof_buffer[eip]++;
		}
#endif
    }
    if (current == task[0] || (--current->counter) <= 0) {
        current->counter = 0;
        need_resched = 1;
    }
    if (current->it_prof_value && !(--current->it_prof_value)) {
		current->it_prof_value = current->it_prof_incr;
		send_sig(SIGPROF,current,1);
        myprint("need %d", current->stime);
	}
    for (mask = 1, tp = timer_table + 0; mask; tp++, mask += mask) {
        if (mask > timer_active)
            break;
        if (!(mask & timer_active))
            continue;
        if (tp->expires > jiffies)
            continue;
        mark_bh(TIMER_BH);
    }
    cli();
    itimer_ticks++;
    if (itimer_ticks > itimer_next)
        need_resched = 1;
    if (next_timer) {
        if (next_timer->expires) {
            next_timer->expires--;
            if (!next_timer->expires)
                mark_bh(TIMER_BH);
        } else {
            lost_ticks++;
            mark_bh(TIMER_BH);
        }
    }
    sti();
}

void sched_init(void) {
    int i;
    struct desc_struct *p;

    bh_base[TIMER_BH].routine = timer_bh;
    if (sizeof(struct sigaction) != 16)
        panic("Struct sigaction MUST be 16 bytes");
    set_tss_desc(gdt + FIRST_TSS_ENTRY, &init_task.tss);
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &default_ldt, 1);
    set_system_gate(0x80, &system_call);
    p = gdt + 2 + FIRST_TSS_ENTRY;
    // 初始化从任务1开始的其他任务
    for (i = 1; i < NR_TASKS; i++) {
        task[i] = NULL;
        p->a = p->b = 0;
        p++;
        p->a = p->b = 0;
        p++;
    }
    // NT标志用于控制程序的嵌套调用，这里复位，不允许
    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl");
    // 将gdt中的tss、ldt加载进tr、ldtr中，只明确加载一次，后面就不需要了
    load_TR(0);
    load_ldt(0);
    outb_p(0x34, 0x43);             /* binary, mode 2, LSB/MSB, ch 0 */
    outb_p(LATCH & 0xff, 0x40);     /* LSB */
    outb(LATCH >> 8, 0x40);         /* MSB */
    if (request_irq(TIMER_IRQ, (void (*)(int)) do_timer) != 0)
        panic("Could not allocate timer IRQ!");
}