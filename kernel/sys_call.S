SIG_CHLD	= 17

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

state	= 0
counter	= 4
priority = 8
signal	= 12
sigaction = 16
blocked = (33*16)

# offsets within sigaction
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

ENOSYS = 38

.globl system_call, timer_interrupt, sys_fork, sys_execve
.globl hd_interrupt, sys_unimpl

.align 4
bad_sys_call:
	pushl $-ENOSYS
	jmp ret_from_sys_call
.align 4
reschedule:
	pushl $ret_from_sys_call
	jmp schedule

.align 4
system_call:
	cld
	pushl %eax		# save the orig_eax
	push %gs
	push %fs
	push %es
	push %ds
	pushl %eax		# save eax, The return value will be put here
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx		
	pushl %ecx		# push %ebx,%ecx,%edx as parameters
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs
	cmpl NR_syscalls, %eax
	jae bad_sys_call
	call sys_call_table(, %eax, 4)
	movl %eax, EAX(%esp)
2:
	movl current, %eax
	cmpl $0, state(%eax)		# 调用完了后发现不是就绪状态
	jne reschedule
	cmpl $0, counter(%eax)		# counter
	je reschedule
ret_from_sys_call:
	movl current, %eax
	cmpl task, %eax			# task[0] cannot have signals
	je 3f
	cmpw $0x0f, CS(%esp)		# 如果是内核态，那么不响应信号
	jne 3f
	cmpw $0x17, OLDSS(%esp)		# 如果是内核栈，也不响应信号
	jne 3f
	movl signal(%eax), %ebx
	movl blocked(%eax), %ecx
	notl %ecx
	andl %ebx, %ecx
	bsfl %ecx, %ecx			# 从位0顺序扫描到位31，将遇到的第一个含1的位保存到ecx
	je 3f
	btrl %ecx, %ebx			# 马上得到处理，将singal即将被处理的位复位
	movl %ebx, signal(%eax)
	incl %ecx				# 将信号调整为1开始的数（即1-32）
	pushl %ecx
	call do_signal
	popl %ecx				
	testl %eax, %eax
	jne 2b					# 如果信号处理函数的返回值不为0就调整到标号2
3:
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
	pop %gs
	addl $4, %esp 		# skip the orig_eax
	iret

.align 4
timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl $-1		# fill in -1 for orig_eax
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	incl jiffies
	# 由于初始化中断控制芯片时没有采用自动 EOI，所以这里需要发指令结束该硬件中断。
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call

.align 4
# oldss oldesp eflags cs eip error_code
sys_execve:
	lea (EIP+4)(%esp),%eax 	# 因为是通过call调用的，所以栈里还有返回地址
	pushl %eax
	call do_execve
	addl $4,%esp
	ret

.align 4
sys_unimpl:
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs
	pushl %eax
	call unimpl
	addl $4, %esp
	ret

hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx,%edx
	movl %edx,hd_timeout
	xchgl do_hd,%edx
	testl %edx,%edx
	jne 1f
	movl $unexpected_hd_interrupt, %edx
1:	outb %al,$0x20
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret