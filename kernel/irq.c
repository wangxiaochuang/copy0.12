#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

static unsigned char cache_21 = 0xff;
static unsigned char cache_A1 = 0xff;

unsigned long intr_count = 0;
unsigned long bh_active = 0;
unsigned long bh_mask = 0xFFFFFFFF;
struct bh_struct bh_base[32];

asmlinkage void do_bottom_half(void) {
	unsigned long active;
	unsigned long mask, left;
	struct bh_struct *bh;
	
	bh = bh_base;
	active = bh_active & bh_mask;
	for (mask = 1, left = ~0; left & active; bh++, mask += mask, left += left) {
		if (mask & active) {
			void (*fn) (void *);
			bh_active &= ~mask;
			fn = bh->routine;
			if (!fn)
				goto bad_bh;
			fn(bh->data);
		}
	}
	return;
bad_bh:
	printk ("irq.c:bad bottom half entry\n");
}

BUILD_IRQ(FIRST,0,0x01)
BUILD_IRQ(FIRST,1,0x02)
BUILD_IRQ(FIRST,2,0x04)
BUILD_IRQ(FIRST,3,0x08)
BUILD_IRQ(FIRST,4,0x10)
BUILD_IRQ(FIRST,5,0x20)
BUILD_IRQ(FIRST,6,0x40)
BUILD_IRQ(FIRST,7,0x80)
BUILD_IRQ(SECOND,8,0x01)
BUILD_IRQ(SECOND,9,0x02)
BUILD_IRQ(SECOND,10,0x04)
BUILD_IRQ(SECOND,11,0x08)
BUILD_IRQ(SECOND,12,0x10)
BUILD_IRQ(SECOND,13,0x20)
BUILD_IRQ(SECOND,14,0x40)
BUILD_IRQ(SECOND,15,0x80)

static void (*interrupt[16])(void) = {
	IRQ0_interrupt, IRQ1_interrupt, IRQ2_interrupt, IRQ3_interrupt,
	IRQ4_interrupt, IRQ5_interrupt, IRQ6_interrupt, IRQ7_interrupt,
	IRQ8_interrupt, IRQ9_interrupt, IRQ10_interrupt, IRQ11_interrupt,
	IRQ12_interrupt, IRQ13_interrupt, IRQ14_interrupt, IRQ15_interrupt
};

static void (*fast_interrupt[16])(void) = {
	fast_IRQ0_interrupt, fast_IRQ1_interrupt,
	fast_IRQ2_interrupt, fast_IRQ3_interrupt,
	fast_IRQ4_interrupt, fast_IRQ5_interrupt,
	fast_IRQ6_interrupt, fast_IRQ7_interrupt,
	fast_IRQ8_interrupt, fast_IRQ9_interrupt,
	fast_IRQ10_interrupt, fast_IRQ11_interrupt,
	fast_IRQ12_interrupt, fast_IRQ13_interrupt,
	fast_IRQ14_interrupt, fast_IRQ15_interrupt
};

static void (*bad_interrupt[16])(void) = {
	bad_IRQ0_interrupt, bad_IRQ1_interrupt,
	bad_IRQ2_interrupt, bad_IRQ3_interrupt,
	bad_IRQ4_interrupt, bad_IRQ5_interrupt,
	bad_IRQ6_interrupt, bad_IRQ7_interrupt,
	bad_IRQ8_interrupt, bad_IRQ9_interrupt,
	bad_IRQ10_interrupt, bad_IRQ11_interrupt,
	bad_IRQ12_interrupt, bad_IRQ13_interrupt,
	bad_IRQ14_interrupt, bad_IRQ15_interrupt
};

static struct sigaction irq_sigaction[16] = {
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL },
	{ NULL, 0, 0, NULL }, { NULL, 0, 0, NULL }
};

asmlinkage void do_IRQ(int irq, struct pt_regs *regs) {
	struct sigaction *sa = irq + irq_sigaction;

	kstat.interrupts++;
	sa->sa_handler((int) regs);
}

asmlinkage void do_fast_IRQ(int irq) {
	struct sigaction *sa = irq + irq_sigaction;

	kstat.interrupts++;
	sa->sa_handler(irq);
}

int irqaction(unsigned int irq, struct sigaction *new_sa) {
	struct sigaction *sa;
	unsigned long flags;

	if (irq > 15)
		return -EINVAL;
	sa = irq + irq_sigaction;
	if (sa->sa_mask)
		return -EBUSY;
	if (!new_sa->sa_handler)
		return -EINVAL;
	save_flags(flags);
	cli();
	*sa = *new_sa;
	sa->sa_mask = 1;
	if (sa->sa_flags & SA_INTERRUPT)
		set_intr_gate(0x20 + irq, fast_interrupt[irq]);
	else
		// 中断执行顺序：IRQ0_interrupt -> do_IRQ -> irq_sigaction[irq].handler(do_timer)
		set_intr_gate(0x20 + irq, interrupt[irq]);
	if (irq < 8) {
		cache_21 &= ~(1<<irq);
		outb(cache_21, 0x21);
	} else {
		cache_21 &= ~(1<<2);
		cache_A1 &= ~(1<<(irq-8));
		outb(cache_21, 0x21);
		outb(cache_A1, 0xA1);
	}
	restore_flags(flags);
	return 0;
}

int request_irq(unsigned int irq, void (*handler)(int)) {
	struct sigaction sa;

	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sa.sa_mask = 0;
	sa.sa_restorer = NULL;
	return irqaction(irq, &sa);	
}

static void math_error_irq(int cpl) {
	outb(0,0xF0);
	if (ignore_irq13)
		return;
	math_error();
}

static void no_action(int cpl) { }

static struct sigaction ignore_IRQ = {
	no_action,
	0,
	SA_INTERRUPT,
	NULL
};

// 32开始的16个是硬件中断
void init_IRQ(void) {
    int i;
    for (i = 0; i < 16; i++)
        set_intr_gate(0x20+i, bad_interrupt[i]);
	if (irqaction(2, &ignore_IRQ))
		printk("Unable to get IRQ2 for cascade\n");
	if (request_irq(13, math_error_irq))
		printk("Unable to get IRQ13 for math-error handler\n");
	for (i = 0; i < 32; i++) {
		bh_base[i].routine = NULL;
		bh_base[i].data = NULL;
	}
	bh_active = 0;
	intr_count = 0;
}