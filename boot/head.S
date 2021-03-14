.code32

#include <linux/tasks.h>
#include <linux/segment.h>

#define CL_MAGIC_ADDR	0x90020
#define CL_MAGIC	0xA33F
#define CL_BASE_ADDR	0x90000
#define CL_OFFSET	0x90022

.globl idt, gdt
.globl swapper_pg_dir
.globl empty_zero_page
.globl startup_32
startup_32:
    cld
    movl $KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    lss stack_start, %esp

    xorl %eax, %eax
    movl $edata, %edi
    movl $end, %ecx
    subl %edi, %ecx     // end - edata = bss
    cld
    rep stosb           // 0 => ds:edi, size: bss
    call setup_idt
    xorl %eax, %eax
1:
    incl %eax
    movl %eax, 0x0
    cmpl %eax, 0x100000
    je 1b
    pushl $0
    popfl               // eflags set 0

    movl $0x90000, %esi
    movl $empty_zero_page, %edi
    movl $512, %ecx
    cld
    rep movsl
    xorl %eax, %eax
    movl $512, %ecx
    rep stosl
    cmpw $CL_MAGIC, CL_MAGIC_ADDR
    jne 1f
    hlt
1:
    movl %esp, %edi
    andl $0xfffffffc, %esp
    movl $3, x86
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $0x40000, %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    xorl %ecx, %eax
    andl $0x40000, %eax
    je is386
    movl $4, x86
    movl %ecx, %eax
    xorl $0x200000, %eax        // check ID flag
    pushl %eax
    popfl
    pushfl
    popl %eax
    xorl %ecx, %eax
    andl $0x200000, %eax
    js is486
isnew:
    pushl %ecx
    popfl
    movl $1, %eax
    cpuid
    andl $0xf00, %eax
    shrl $8, %eax
    movl %eax, x86
    movl %edi, %esp     // restore esp
    movl %cr0, %eax
    andl $0x80000011, %eax  // save PG, PE, ET
    orl $0x50022, %eax      // set AM, WP, NE and MP
    jmp 2f

is486:
    hlt

is386:
    hlt

2:
    movl %eax, %cr0
    call check_x87
    call setup_paging
    lgdt gdt_descr
    lidt idt_descr
    jmp $KERNEL_CS, $1f
1:
    movl $KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    lss stack_start, %esp

    xorl %eax, %eax
    lldt %ax
    pushl %eax
    pushl %eax
    pushl %eax
    cld
    call start_kernel
L6:
    jmp L6
    



check_x87:
    movl $0, hard_math
    clts
    fninit
    fstsw %ax
    cmpb $0, %al
    je 1f
    movl %cr0, %eax
    xorl $4, %eax       // set EM
    movl %eax, %cr0
    ret
.align 2
1:
    movl $1, hard_math
    fsetpm              // 只对287生效
    ret


setup_idt:
    lea ignore_int, %edx
    movl $KERNEL_CS<<16, %eax
    movw %dx, %ax
    movw $0x8e00, %dx           // interrupt gate - dpl=0, present

    lea idt, %edi
    mov $256, %ecx
rp_sidt:
    movl %eax, (%edi)
    movl %edx, 4(%edi)
    addl $8, %edi
    dec %ecx
    jne rp_sidt
    ret

    

.align 2
setup_paging:
    movl $1024*2, %ecx      // 2 pages - swapper_pg_dir+1 page table
    xorl %eax, %eax
    movl $swapper_pg_dir, %edi  // 0 => es:edi 2 * 4k
    cld; rep; stosl
    // 页目录表
    movl $pg0+7, swapper_pg_dir
    // 3G空间的页目录表
    movl $pg0+7, swapper_pg_dir+3072        // 0xC0000000
    // pg0这一页表可以表示1024个页，正好对应最前面的4M空间
    // 从后往前设置，最后一个页的物理地址是 4M-4096
    movl $pg0+4092, %edi
    movl $0x03ff007, %eax           // 4M - 4096 + 7
    std
1:
    stosl
    subl $0x1000, %eax
    jge 1b
    cld
    movl $swapper_pg_dir, %eax
    movl %eax, %cr3                 // page directory start
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0                 // enable paging
    ret
    
.org 0x1000
swapper_pg_dir:

.org 0x2000
pg0:

.org 0x3000
empty_bad_page:

.org 0x4000
empty_bad_page_table:

.org 0x5000
empty_zero_page:

.org 0x6000

tmp_floppy_area:
    .fill 1024, 1, 0

floppy_track_buffer:
    .fill 512*2*18, 1, 0

int_msg:
    .asciz "unknown interrupt\n"

.align 2
ignore_int:
    cld
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $KERNEL_DS, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	pushl $int_msg
	call printk
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret



.align 4
.word 0
idt_descr:
    .word 256*8-1       
    .long 0xc0000000+idt

.align 4
idt:
    .fill 256, 8, 0

.align 4
.word 0
gdt_descr:
    .word (8+2*NR_TASKS)*8-1
    .long 0xc0000000+gdt

.align 4
gdt:
    .quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x0000000000000000	/* not used */
	.quad 0xc0c39a000000ffff	/* 0x10 kernel 1GB code at 0xC0000000 */
	.quad 0xc0c392000000ffff	/* 0x18 kernel 1GB data at 0xC0000000 */
	.quad 0x00cbfa000000ffff	/* 0x23 user   3GB code at 0x00000000 */
	.quad 0x00cbf2000000ffff	/* 0x2b user   3GB data at 0x00000000 */
	.quad 0x0000000000000000	/* not used */
	.quad 0x0000000000000000	/* not used */
	.fill 2*NR_TASKS,8,0		/* space for LDT's and TSS's etc */