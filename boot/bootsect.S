#include <linux/config.h>

BOOTSEG  = 0x07c0
INITSEG  = DEF_INITSEG
SYSSEG   = DEF_SYSSEG
SETUPSEG = DEF_SETUPSEG

.code16
entry:
    mov $BOOTSEG, %ax
    mov %ax, %ds
    mov $INITSEG, %ax
    mov %ax, %es
    mov $256, %cx
    xor %si, %si
    xor %di, %di
    rep movsw
    jmp $INITSEG, $go

go:
    mov %cs, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0xfef4, %dx
    mov %dx, %sp

    call clear_screen

    mov $msg1, %si
    call print

    # load setup 4 sector to 90200 from 1 offset
    mov $4, %cx
    mov $0x1, %eax
    mov $0x0200, %ebx
    1:
        call disk_load
        add $1, %eax
    loop 1b

    # load system 1024 sector to 10000 from 5 offset
    mov $SYSSEG, %ax
    mov %ax, %ds

    mov $10, %cx
    mov $0x5, %eax
    mov $0, %ebx
    1:
        call disk_load
        add $1, %eax
    loop 1b

    jmp $SETUPSEG, $0
    jmp .

clear_screen:
    push %ax
    mov $0x3, %ax
    int $0x10
    pop %ax
    ret

# ------------------
# mov $MSG, %si
# call print
print:
    cld
    push %ax
    mov $0x0e, %ah
    1:
        lodsb
        test %al, %al
        jz 2f
        int $0x10
        jmp 1b
    2:
        pop %ax
        ret

# ------------------
# eax LBA的扇区号
# ebx 数据写入的内存地址
disk_load:
    push %eax
    push %ecx
    push %edx

    push %eax
    # 读写硬盘
	mov $0x1f2, %dx
	mov $1, %al         //每次就读一个扇区
	out %al, %dx

    # 将LBA的地址存入0x1f3，0x1f6

    pop %eax
	# 7-0位写入0x1f3
	mov $0x1f3, %dx
	out %al, %dx

	# 15-8位写给1f4
	mov $8, %cl
	shr %cl, %eax
	mov $0x1f4, %dx
	out %al, %dx

	# 23-16位写给1f5
	shr %cl, %eax
	mov $0x1f5, %dx
	out %al, %dx

	shr %cl, %eax
	and $0x0f, %al
	or $0xe0, %al	# 设置7-4位为1110，此时才是lBA模式
	mov $0x1f6, %dx
	out %al, %dx

	# 向0x1f7写入读命令
	mov $0x1f7, %dx
	mov $0x20, %al
	out %al, %dx

	# 检测硬盘状态
	.not_ready:
	nop
	in %dx, %al
	and $0x88, %al # 4位为1，表示可以传输，7位为1表示硬盘忙
	cmp $0x08, %al
	jnz .not_ready

	# 读数据
    mov $256, %ecx
	mov $0x1f0, %dx

	.go_on:
		in %dx, %ax
		mov %ax, (%ebx)
		add $2, %ebx
		loop .go_on
    pop %edx
    pop %ecx
    pop %eax
    ret

msg1:
    .byte 13, 10
    .asciz "bootsect running..."

LEN = (510 - (. - entry))
.fill LEN, 1, 0
.word 0xaa55
