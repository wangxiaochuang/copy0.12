.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, irq13, reserved
.globl alignment_check

divide_error:
	pushl $do_divide_error
error_code:
	push %fs
	push %es
	push %ds
	pushl %eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	cld
	movl $-1, %eax
	xchgl %eax, 0x2c(%esp)		//[ori_ss ori_esp] eflags cs eip error_code func
	xorl %ebx, %ebx
	mov %gs, %bx
	xchgl %ebx, 0x28(%esp)		// 中断处理函数放到ebx，保存gs
	pushl %eax					// error_code 压栈
	lea 0x34(%esp), %edx		// 此时edx为栈中eip的地址
	pushl %edx
	movl $0x10, %edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	call *%ebx
	addl $8, %esp
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
	pop %ds
	pop %es
	pop %fs
	pop %gs						// 这个位置之前是处理函数，被交换成gs了
	addl $4, %esp				// 跳过了error_code，可能是处理器要求，处理器压入的，程序来释放 @todo
	iret

debug:
	pushl $0
	pushl $do_int3		# _do_debug
	jmp error_code

nmi:
	pushl $0
	pushl $do_nmi
	jmp error_code

int3:
	pushl $0
	pushl $do_int3
	jmp error_code

overflow:
	pushl $0
	pushl $do_overflow
	jmp error_code

bounds:
	pushl $0
	pushl $do_bounds
	jmp error_code

invalid_op:
	pushl $0
	pushl $do_invalid_op
	jmp error_code

coprocessor_segment_overrun:
	pushl $0
	pushl $do_coprocessor_segment_overrun
	jmp error_code

reserved:
	pushl $0
	pushl $do_reserved
	jmp error_code

irq13:
	pushl %eax
	xorb %al, %al
	outb %al, $0xF0
	movb $0x20, %al
	outb %al, $0x20
	jmp 1f
1:	jmp 1f
1:	outb %al, $0xA0
	popl %eax
	jmp coprocessor_error

double_fault:
	pushl $do_double_fault
	jmp error_code

invalid_TSS:
	pushl $do_invalid_TSS
	jmp error_code

segment_not_present:
	pushl $do_segment_not_present
	jmp error_code

stack_segment:
	pushl $do_stack_segment
	jmp error_code

general_protection:
	pushl $do_general_protection
	jmp error_code

alignment_check:
	pushl $do_alignment_check
	jmp error_code

page_fault:
	pushl $do_page_fault
	jmp error_code