#include <linux/segment.h>

EBX		= 0x00
ECX		= 0x04
EDX		= 0x08
ESI		= 0x0C
EDI		= 0x10
EBP		= 0x14
EAX		= 0x18
DS		= 0x1C
ES		= 0x20
FS		= 0x24
GS		= 0x28
ORIG_EAX	= 0x2C
EIP		= 0x30
CS		= 0x34
EFLAGS		= 0x38
OLDESP		= 0x3C
OLDSS		= 0x40

CF_MASK		= 0x00000001
IF_MASK		= 0x00000200
NT_MASK		= 0x00004000
VM_MASK		= 0x0002000

/*
 * these are offsets into the task-struct.
 */
state		=  0
counter		=  4
priority	=  8
signal		= 12
blocked		= 16
flags		= 20
errno		= 24
dbgreg6		= 52
dbgreg7		= 56

ENOSYS = 38

.globl system_call, lcall7
.globl device_not_available, coprocessor_error
.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, reserved
.globl alignment_check, page_fault
.globl ret_from_sys_call

#define SAVE_ALL \
	cld; \
	push %gs; \
	push %fs; \
	push %es; \
	push %ds; \
	pushl %eax; \
	pushl %ebp; \
	pushl %edi; \
	pushl %esi; \
	pushl %edx; \
	pushl %ecx; \
	pushl %ebx; \
	movl $(KERNEL_DS),%edx; \
	mov %dx,%ds; \
	mov %dx,%es; \
	movl $(USER_DS),%edx; \
	mov %dx,%fs;

#define RESTORE_ALL \
	cmpw $(KERNEL_CS),CS(%esp); \
	je 1f;   \
	movl current,%eax; \
	movl dbgreg7(%eax),%ebx; \
	movl %ebx,%db7;	\
1:	popl %ebx; \
	popl %ecx; \
	popl %edx; \
	popl %esi; \
	popl %edi; \
	popl %ebp; \
	popl %eax; \
	pop %ds; \
	pop %es; \
	pop %fs; \
	pop %gs; \
	addl $4,%esp; \
	iret

.align 4
lcall7:
    hlt

.align 4
handle_bottom_half:
	pushfl
	incl intr_count
	sti
	call do_bottom_half
	popfl
	decl intr_count
	jmp 9f
.align 4
reschedule:
	pushl $ret_from_sys_call
	jmp schedule
.align 4
system_call:
    pushl %eax
    SAVE_ALL
    movl $-ENOSYS, EAX(%esp)
    cmpl NR_syscalls, %eax
    jae ret_from_sys_call
    movl current, %ebx
    andl $~CF_MASK, EFLAGS(%esp)    // clear carry - assume no errors
    movl $0, errno(%ebx)
    movl %db6, %edx
    movl %edx, dbgreg6(%ebx)        // 保存当前进程硬件调试状态
    testb $0x20, flags(%ebx)         // PF_TRACESYS
    jne 1f
    call sys_call_table(, %eax, 4)
    movl %eax, EAX(%esp)            // 保存系统调用的返回码
    movl errno(%ebx), %edx
    negl %edx                       // 错误码是正数，这里变成负数
    je ret_from_sys_call
    movl %edx, EAX(%esp)
    orl $CF_MASK, EFLAGS(%esp)      // 设置carry标志，表示有错误
    jmp ret_from_sys_call
.align 4
1:
    // call syscall_trace

.align 4, 0x90
ret_from_sys_call:
    cmpl $0, intr_count
    jne 2f
	movl bh_mask, %eax
	andl bh_active, %eax
	jne handle_bottom_half
9:
	movl EFLAGS(%esp), %eax
	testl $VM_MASK, %eax
	jne 1f
	cmpw $KERNEL_CS, CS(%esp)
	je 2f
1:
	sti
	orl $IF_MASK, %eax
	andl $~NT_MASK, %eax
	movl %eax, EFLAGS(%esp)
	cmpl $0, need_resched
	jne reschedule
	movl current, %eax
	cmpl task, %eax
	je 2f
	cmpl $0, state(%eax)
	jne reschedule
	cmpl $0, counter(%eax)
	je reschedule
	movl blocked(%eax), %ecx
	movl %ecx, %ebx
	notl %ecx
	andl signal(%eax), %ecx
	jne signal_return
2:
    RESTORE_ALL
.align 4
signal_return:
    movl %esp, %ecx
    pushl %ecx
    testl $VM_MASK, EFLAGS(%ecx)
    jne v86_signal_return
    pushl %ebx
    call do_signal
    popl %ebx
    popl %ebx
    RESTORE_ALL
.align 4
v86_signal_return:
    call save_v86_state
    movl %eax, %esp
    pushl %eax
    pushl %ebx
    call do_signal
    popl %ebx
    popl %ebx
    RESTORE_ALL

.align 4
divide_error:
    pushl $0
    pushl $do_divide_error
.align 4, 0x90
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
	movl $0,%eax
	movl %eax,%db7			# disable hardware debugging...
	cld
	movl $-1, %eax
	xchgl %eax, ORIG_EAX(%esp)	# orig_eax (get the error code. )
	xorl %ebx,%ebx			# zero ebx
	mov %gs,%bx			# get the lower order bits of gs
	xchgl %ebx, GS(%esp)		# get the address and save gs.
	pushl %eax			# push the error code
	lea 4(%esp),%edx
	pushl %edx
	movl $(KERNEL_DS),%edx
	mov %dx,%ds
	mov %dx,%es
	movl $(USER_DS),%edx
	mov %dx,%fs
	pushl %eax
	movl current,%eax
	movl %db6,%edx
	movl %edx,dbgreg6(%eax)  # save current hardware debugging status
	popl %eax
	call *%ebx
	addl $8,%esp
	jmp ret_from_sys_call

.align 4
coprocessor_error:
	pushl $0
	pushl $do_coprocessor_error
	jmp error_code

.align 4
device_not_available:
	pushl $-1		# mark this as an int
	SAVE_ALL
	pushl $ret_from_sys_call
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je math_state_restore
	pushl $0		# temporary storage for ORIG_EIP
	call math_emulate
	addl $4,%esp
	ret

.align 4
debug:
	pushl $0
	pushl $do_debug
	jmp error_code

.align 4
nmi:
	pushl $0
	pushl $do_nmi
	jmp error_code

.align 4
int3:
	pushl $0
	pushl $do_int3
	jmp error_code

.align 4
overflow:
	pushl $0
	pushl $do_overflow
	jmp error_code

.align 4
bounds:
	pushl $0
	pushl $do_bounds
	jmp error_code

.align 4
invalid_op:
	pushl $0
	pushl $do_invalid_op
	jmp error_code

.align 4
coprocessor_segment_overrun:
	pushl $0
	pushl $do_coprocessor_segment_overrun
	jmp error_code

.align 4
reserved:
	pushl $0
	pushl $do_reserved
	jmp error_code

.align 4
double_fault:
	pushl $do_double_fault
	jmp error_code

.align 4
invalid_TSS:
	pushl $do_invalid_TSS
	jmp error_code

.align 4
segment_not_present:
	pushl $do_segment_not_present
	jmp error_code

.align 4
stack_segment:
	pushl $do_stack_segment
	jmp error_code

.align 4
general_protection:
	pushl $do_general_protection
	jmp error_code

.align 4
alignment_check:
	pushl $do_alignment_check
	jmp error_code

.align 4
page_fault:
	pushl $do_page_fault
	jmp error_code