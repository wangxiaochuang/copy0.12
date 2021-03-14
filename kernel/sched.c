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

long tick = 1000000 / HZ;               /* timer interrupt period */
volatile struct timeval xtime;          // the current time
int tickadj = 500/HZ;                   // microsecs

int need_resched = 0;

int hard_math = 0;
int x86 = 0;
int ignore_irq13 = 0;

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

fn_ptr sys_call_table[] = {0};
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
    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl");
    load_TR(0);
    load_ldt(0);
    outb_p(0x34, 0x43);             /* binary, mode 2, LSB/MSB, ch 0 */
    outb_p(LATCH & 0xff, 0x40);     /* LSB */
    outb(LATCH >> 8, 0x40);         /* MSB */
    if (request_irq(TIMER_IRQ, (void (*)(int)) do_timer) != 0)
        panic("Could not allocate timer IRQ!");
}