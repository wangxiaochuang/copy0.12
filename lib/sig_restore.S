.globl ___sig_restore
.globl ___masksig_restore

___sig_restore:
	addl $4,%esp	# signr
	popl %eax
	popl %ecx
	popl %edx
	popfl
	ret

___masksig_restore:
	addl $4,%esp		# signr
	call ___ssetmask	# old blocking
	addl $4,%esp
	popl %eax
	popl %ecx
	popl %edx
	popfl
	ret