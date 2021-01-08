#include <linux/config.h>

INITSEG  = DEF_INITSEG
SYSSEG   = DEF_SYSSEG
SETUPSEG = DEF_SETUPSEG

stack_start = 0x700000

.code32
.globl idt, gdt
pg_dir:

.globl startup_32
startup_32:
    movl $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs
    mov %eax, %gs
    mov %eax, %ss
    mov $stack_start, %esp

    call setup_idt
    lgdt gdt_descr

    movl $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov $stack_start, %esp

    jmp after_page_tables

setup_idt:
    // 0-1: offset, 2-3: selector
    lea ignore_int, %edx
    movl $0x00080000, %eax
    movw %dx, %ax
    // 4-5: flag, 6-7: offset
    movw $0x8E00, %dx

    lea idt, %edi
    mov $256, %ecx
    rp_sidt:
        movl %eax, (%edi)
        movl %edx, 4(%edi)
        addl $8, %edi
        dec %ecx
        jnz rp_sidt
        lidt idt_descr
    ret

.org 0x1000
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000

ignore_int:
    pushl %eax
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    push %fs

    movl $0x10, %eax
    mov %eax, %ds
    mov %eax, %es
    mov %eax, %fs

    // call printk
    // popl %eax

    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    iret

after_page_tables:
    pushl $0
    pushl $0
    pushl $0
    pushl $L6
    pushl $main
    jmp setup_paging
L6:
    jmp L6

setup_paging:
    // 一个页目录，四个页表
    movl $1024 * 5, %ecx
    xorl %eax, %eax
    xorl %edi, %edi

    cld; rep; stosl

    movl $pg0 + 7, pg_dir
    movl $pg1 + 7, pg_dir + 4
    movl $pg2 + 7, pg_dir + 8
    movl $pg3 + 7, pg_dir + 12

    movl $pg3 + 4092, %edi
    movl $0x00fff007, %eax
    std
    1:
        stosl
        subl $0x1000, %eax
        jge 1b
    cld
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    ret

.align 4
.word 0
idt_descr:
    .word 256 * 8 - 1
    .long idt

.align 4
.word 0
gdt_descr:
    .word 256 * 8 - 1
    .long gdt

.align 8
idt:
    .fill 256, 8, 0
gdt:
    .quad 0x0000000000000000			/* NULL descriptor */
    .quad 0x00c09a0000000fff			/* 16Mb */		# 0x08，内核代码段，长度16MB
    .quad 0x00c0920000000fff			/* 16Mb */		# 0x10，内核数据段，长度16MB
    .quad 0x0000000000000000			/* TEMPORARY - don't use */
    .fill 252, 8, 0

msg1:
    .byte 13, 10
    .asciz "system Loading..."
