#include <linux/sched.h>
#include <asm/system.h>
#include <signal.h>

#define LATCH (1193180 / HZ)

union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK, };

unsigned long startup_time = 0;

struct task_struct *task[NR_TASKS] = {&(init_task.task), };

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

    outb_p(0x36,0x43);				/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);		/* MSB */
}