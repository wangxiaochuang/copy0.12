.code32

#include <linux/tasks.h>
#include <linux/segment.h>

#define CL_MAGIC_ADDR	0x90020
#define CL_MAGIC	0xA33F
#define CL_BASE_ADDR	0x90000
#define CL_OFFSET	0x90022

.globl startup_32
startup_32:
    mov $0, %ax
    cld
    movl $KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    jmp .
    mov $0, %eax