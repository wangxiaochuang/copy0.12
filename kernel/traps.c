#include <linux/head.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

/**
 * 取seg中地址addr处的一个字节
 * @param[in]	seg		段选择符
 * @param[in]	addr	段内指定地址
 */
#define get_seg_byte(seg, addr) ({ 									\
	register char __res; 											\
	__asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" 	\
		:"=a" (__res):"0" (seg),"m" (*(addr))); 					\
	__res;})

/* 取seg中地址addr处的一个长字(4字节) */
#define get_seg_long(seg,addr) ({ 									\
	register unsigned long __res;	 								\
	__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" 	\
		:"=a" (__res):"0" (seg),"m" (*(addr))); 					\
	__res;})

/* 取fs段寄存器的值(选择符) */
#define _fs() ({ 													\
	register unsigned short __res; 									\
	__asm__("mov %%fs,%%ax":"=a" (__res):); 						\
	__res;})

void page_exception(void);					/* 页异常，实际是page_fault(mm/page.s) */

void divide_error(void);					// int0(kernel/asm.s)
void debug(void);							// int1(kernel/asm.s)
void nmi(void);								// int2(kernel/asm.s)
void int3(void);							// int3(kernel/asm.s)
void overflow(void);						// int4(kernel/asm.s)
void bounds(void);							// int5(kernel/asm.s)
void invalid_op(void);						// int6(kernel/asm.s)
void device_not_available(void);			// int7(kernel/sys_call.s)
void double_fault(void);					// int8(kernel/asm.s)
void coprocessor_segment_overrun(void);		// int9(kernel/asm.s)
void invalid_TSS(void);						// int10(kernel/asm.s)
void segment_not_present(void);				// int11(kernel/asm.s)
void stack_segment(void);					// int12(kernel/asm.s)
void general_protection(void);				// int13(kernel/asm.s)
void page_fault(void);						// int14(mm/page.s)
void coprocessor_error(void);				// int16(kernel/sys_call.s)
void reserved(void);						// int15(kernel/asm.s)
void parallel_interrupt(void);				// int39(kernel/sys_call.s)
void irq13(void);							// int45协处理器中断处理(kernel/asm.s)
void alignment_check(void);					// int46(kernel/asm.s)

static void die(char * str, long esp_ptr, long nr)
{
	// long * esp = (long *) esp_ptr;
	printk("%s: %04x\n\r",str,nr&0xffff);
	printk("base: %p, limit: %p\n", get_base(current->ldt[1]), get_limit(0x17));
	panic("die: %s\n\r", str);
}

/* 以下以do_开头的函数是asm.s中对应中断处理程序调用的C函数 */
void do_double_fault(long esp, long error_code)
{
	die("double fault", esp, error_code);
}

void do_general_protection(long esp, long error_code)
{
	die("general protection", esp, error_code);
}

void do_alignment_check(long esp, long error_code)
{
    die("alignment check", esp, error_code);
}

void do_divide_error(long esp, long error_code)
{
	die("divide error", esp, error_code);
}

/* 参数是进入中断后被顺序压入堆栈的寄存器值 */
void do_int3(long * esp, long error_code,
		long fs, long es, long ds,
		long ebp, long esi, long edi,
		long edx, long ecx, long ebx, long eax)
{}

void do_nmi(long esp, long error_code)
{
	die("nmi", esp, error_code);
}

void do_debug(long esp, long error_code)
{
	die("debug", esp, error_code);
}

void do_overflow(long esp, long error_code)
{
	die("overflow", esp, error_code);
}

void do_bounds(long esp, long error_code)
{
	die("bounds", esp, error_code);
}

void do_invalid_op(long esp, long error_code)
{
	die("invalid operand", esp, error_code);
}

void do_device_not_available(long esp, long error_code)
{
	die("device not available", esp, error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code)
{
	die("coprocessor segment overrun", esp, error_code);
}

void do_invalid_TSS(long esp, long error_code)
{
	die("invalid TSS", esp, error_code);
}

void do_segment_not_present(long esp, long error_code)
{
	die("segment not present", esp, error_code);
}

void do_stack_segment(long esp, long error_code)
{
	die("stack segment", esp, error_code);
}

void do_coprocessor_error(long esp, long error_code)
{
	die("coprocessor error", esp, error_code);
}

void do_reserved(long esp, long error_code)
{
	die("reserved (15,17-47) error", esp, error_code);
}

void trap_init(void)
{
    int i;

    set_trap_gate(0, &divide_error);
    set_trap_gate(1, &debug);
    set_trap_gate(2, &nmi);
    set_system_gate(3, &int3);
    set_system_gate(4, &overflow);
    set_system_gate(5, &bounds);
    set_trap_gate(6, &invalid_op);
    // set_trap_gate(7, &device_not_available);
    set_trap_gate(8, &double_fault);
    set_trap_gate(9, &coprocessor_segment_overrun);
    set_trap_gate(10, &invalid_TSS);
    set_trap_gate(11, &segment_not_present);
    set_trap_gate(12, &stack_segment);
    set_trap_gate(13, &general_protection);
    set_trap_gate(14, &page_fault);
    set_trap_gate(15, &reserved);
    // set_trap_gate(16, &coprocessor_error);
    set_trap_gate(17, &alignment_check);
    for (i = 18; i < 48; i++) {
        set_trap_gate(i, &reserved);
    }
    // set_trap_gate(45, &irq13);
    outb_p(inb_p(0x21)&0xfb, 0x21);
    outb(inb_p(0xA1)&0xdf, 0xA1);
    // set_trap_gate(39, &parallel_interrupt);
}
