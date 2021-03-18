#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#define __STR(x) #x
#define STR(x) __STR(x)
 
#define SAVE_ALL \
	"cld\n\t" \
	"push %gs\n\t" \
	"push %fs\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %ebp\n\t" \
	"pushl %edi\n\t" \
	"pushl %esi\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t" \
	"movl $" STR(USER_DS) ",%edx\n\t" \
	"mov %dx,%fs\n\t"   \
	"movl $0,%edx\n\t"  \
	"movl %edx,%db7\n\t"

#define SAVE_MOST \
	"cld\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t"

#define RESTORE_MOST \
	"popl %ecx\n\t" \
	"popl %edx\n\t" \
	"popl %eax\n\t" \
	"pop %ds\n\t" \
	"pop %es\n\t" \
	"iret"

/**
 * 完全嵌套方式，中断优先级固定，发送普通中断结束（EOI）指令就自动清除当前ISR中优先级最高位置
 * 因此这里都是写入0x20到0x20端口
 **/
#define ACK_FIRST(mask) \
	"inb $0x21, %al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ",cache_21\n\t" /* 屏蔽对应中断 */ \
	"movb cache_21, %al\n\t" \
	"outb %al, $0x21\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20, %al\n\t" /* 发送EOI指令结束该硬件中断，主芯片端口地址0x20 */ \		
	"outb %al, $0x20\n\t"

#define ACK_SECOND(mask) \
	"inb $0xA1, %al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ", cache_A1\n\t" \
	"movb cache_A1, %al\n\t" \
	"outb %al, $0xA1\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20, %al\n\t" \
	"outb %al, $0xA0\n\t" /* 发送EOI指令结束该硬件中断，从芯片端口地址0xA0 */ \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\toutb %al, $0x20\n\t"

#define UNBLK_FIRST(mask) \
	"inb $0x21, %al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~" #mask ", cache_21\n\t" /* 允许对应中断 */ \
	"movb cache_21, %al\n\t" \
	"outb %al, $0x21\n\t"

#define UNBLK_SECOND(mask) \
	"inb $0xA1, %al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~" #mask ", cache_A1\n\t" \
	"movb cache_A1, %al\n\t" \
	"outb %al, $0xA1\n\t"

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)
#define FAST_IRQ_NAME(nr) IRQ_NAME2(fast_IRQ##nr)
#define BAD_IRQ_NAME(nr) IRQ_NAME2(bad_IRQ##nr)

/**
 * 
 * 
 **/
#define BUILD_IRQ(chip, nr, mask) \
asmlinkage void IRQ_NAME(nr); \
asmlinkage void FAST_IRQ_NAME(nr); \
asmlinkage void BAD_IRQ_NAME(nr); \
__asm__( \
"\n.align 4\n" \
"IRQ" #nr "_interrupt:\n\t" \
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ACK_##chip(mask) \
	"incl intr_count\n\t" \
	"sti\n\t" \
	"movl %esp, %ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call do_IRQ\n\t" \
	"addl $8, %esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl intr_count\n\t" \
	"jmp ret_from_sys_call\n" \
"\n.align 4\n" \
"fast_IRQ" #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	"incl intr_count\n\t" \
	"pushl $" #nr "\n\t" \
	"call do_fast_IRQ\n\t" \
	"addl $4, %esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl intr_count\n\t" \
	RESTORE_MOST \
"\n\n.align 4\n" \
"bad_IRQ" #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	RESTORE_MOST);

#endif