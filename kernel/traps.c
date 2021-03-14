#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/segment.h>
#include <linux/ptrace.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#define DO_ERROR(trapnr, signr, str, name, tsk) \
asmlinkage void do_##name(struct pt_regs *regs, long error_code) { \
    tsk->tss.error_code = error_code; \
    tsk->tss.trap_no = trapnr; \
    if (signr == SIGTRAP && current->flags & PF_PTRACED) \
        current->blocked &= ~(1 << (SIGTRAP-1)); \
    send_sig(signr, tsk, 1); \
    die_if_kernel(str, regs, error_code); \
}

void page_exception(void);

asmlinkage void divide_error(void);
asmlinkage void debug(void);
asmlinkage void nmi(void);
asmlinkage void int3(void);
asmlinkage void overflow(void);
asmlinkage void bounds(void);
asmlinkage void invalid_op(void);
asmlinkage void device_not_available(void);
asmlinkage void double_fault(void);
asmlinkage void coprocessor_segment_overrun(void);
asmlinkage void invalid_TSS(void);
asmlinkage void segment_not_present(void);
asmlinkage void stack_segment(void);
asmlinkage void general_protection(void);
asmlinkage void page_fault(void);
asmlinkage void coprocessor_error(void);
asmlinkage void reserved(void);
asmlinkage void alignment_check(void);

void die_if_kernel(char * str, struct pt_regs * regs, long err) {

}

DO_ERROR( 0, SIGFPE,  "divide error", divide_error, current)
DO_ERROR( 3, SIGTRAP, "int3", int3, current)
DO_ERROR( 4, SIGSEGV, "overflow", overflow, current)
DO_ERROR( 5, SIGSEGV, "bounds", bounds, current)
DO_ERROR( 6, SIGILL,  "invalid operand", invalid_op, current)
DO_ERROR( 7, SIGSEGV, "device not available", device_not_available, current)
DO_ERROR( 8, SIGSEGV, "double fault", double_fault, current)
DO_ERROR( 9, SIGFPE,  "coprocessor segment overrun", coprocessor_segment_overrun, last_task_used_math)
DO_ERROR(10, SIGSEGV, "invalid TSS", invalid_TSS, current)
DO_ERROR(11, SIGSEGV, "segment not present", segment_not_present, current)
DO_ERROR(12, SIGSEGV, "stack segment", stack_segment, current)
DO_ERROR(13, SIGSEGV, "general protection", general_protection, current)
DO_ERROR(15, SIGSEGV, "reserved", reserved, current)
DO_ERROR(17, SIGSEGV, "alignment check", alignment_check, current)

asmlinkage void do_nmi(struct pt_regs * regs, long error_code) {}

asmlinkage void do_debug(struct pt_regs * regs, long error_code) {}

void math_error(void) {}

asmlinkage void do_coprocessor_error(struct pt_regs * regs, long error_code) {}

void trap_init(void) {
    int i;

    set_trap_gate(0, &divide_error);
    set_trap_gate(1, &debug);
    set_trap_gate(2, &nmi);
    set_system_gate(3,&int3);	/* int3-5 can be called from all */
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
    set_trap_gate(6, &invalid_op);
    set_trap_gate(7, &device_not_available);
    set_trap_gate(8, &double_fault);
    set_trap_gate(9, &coprocessor_segment_overrun);
    set_trap_gate(10, &invalid_TSS);
    set_trap_gate(11, &segment_not_present);
    set_trap_gate(12, &stack_segment);
    set_trap_gate(13, &general_protection);
    set_trap_gate(14, &page_fault);
    set_trap_gate(15, &reserved);
    set_trap_gate(16, &coprocessor_error);
    set_trap_gate(17, &alignment_check);
    for (i = 18; i < 48; i++)
        set_trap_gate(i, &reserved);
}