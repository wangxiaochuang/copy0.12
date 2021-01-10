#include <linux/config.h>

INITSEG  = DEF_INITSEG
SYSSEG   = DEF_SYSSEG
SETUPSEG = DEF_SETUPSEG

.code16
entry:
    mov $INITSEG, %ax
    mov %ax, %ds

//;;;; 1.获取系统初始化所需要的参数 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// 内存地址	 ; 字节 ; 内容				 ;
// 0x90000	; 2	;   光标位置		    ;
// 0x90002	; 2	;	扩展内存大小 		 ;
// 0x90004	; 2	;	显示页面			;
// 0x90006	; 1 ;	显示模式			;
// 0x90007	; 1 ;	字符列数			;
// 0x90008	; 2	;	??				   ;
// 0x9000A	; 1	;	安装的显示内存		 ;
// 0x9000B	; 1	;	显示状态(彩色/单色)	 ;
// 0x9000C	; 2	;	显示卡特性参数		 ;
// 0x9000E	; 1	;	屏幕当前行值		 ;
// 0x9000F	; 1	;	屏幕当前列值		 ;

    # 取从0x100000（1M）处开始的扩展内存大小（KB）
    mov $0x88, %ah
    int $0x15
    movw %ax, 2

    # 检查EGA/VGA和一些配置参数
    mov $0x12, %ah
    mov $0x10, %bl
    int $0x10
    mov %ax, 8
    mov %bx, 10
    mov %cx, 12
    # 检测当前行列值
    mov $0x5019, %ax        // row: 80, col: 25
    cmp $0x10, %bl
    je novga
novga:
    mov %ax, 14
    # 读取光标位置
    mov $0x03, %ah
    xor %bh, %bh
    int $0x10
    movw %dx, 0
    # 获取显示卡当前显示模式
    mov $0x0f, %ah
    int $0x10
    mov %bx, 4
    mov %ax, 6

    cli
    # protect mode
    xor %ax, %ax
    cld
do_move:
    mov %ax, %es
    add $0x1000, %ax
    cmp $0x9000, %ax
    jz end_move
    mov %ax, %ds
    xor %di, %di
    xor %si, %si
    mov $0x8000, %cx
    rep movsw
    jmp do_move
end_move:
    mov $SETUPSEG, %ax
    mov %ax, %ds
    lidt idt_48
    lgdt gdt_48

    movl %cr0, %eax
    or $1, %eax
	movl %eax, %cr0

    // 8259A 重编程
    mov $0x11, %al
    out %al, $0x20
    .word 0x00eb, 0x00eb
    out %al, $0xa0
    .word 0x00eb, 0x00eb
    mov $0x20, %al
    out %al, $0x21
    .word 0x00eb, 0x00eb
    mov $0x28, %al
    out %al, $0xa1
    .word 0x00eb, 0x00eb
    mov $0x04, %al
    out %al, $0x21
    .word 0x00eb, 0x00eb
    mov $0x02, %al
    out %al, $0xa1
    .word 0x00eb, 0x00eb
    mov $0x01, %al
    out %al, $0x21
    .word 0x00eb, 0x00eb
    out %al, $0xa1
    .word 0x00eb, 0x00eb

    mov $0xff, %al
    out %al, $0x21
    .word 0x00eb, 0x00eb
    out %al, $0xa1

    jmp $8, $0
    hlt

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
# mov $MSG, %si
# call print_next
# .string "this is my loader"
print_next:
    xchg %si, %ss:(%esp)
    push %ax
    mov $0x0e, %ah
    1:
        mov %ds:(%si), %al
        inc %si
        test %al, %al
        jz 2f
        int $0x10
        jmp 1b
    2:
        pop %ax
        xchg %si, %ss:(%esp)
        ret

gdt:
    .quad  0
    .quad 0x00c09a00000007ff
    .quad 0x00c09200000007ff

idt_48:
    .word 0
    .word 0, 0

gdt_48:
    .word 0x800         # gdt limit=2048, 256 GDT entries
    .word 512+gdt, 0x9  # gdt base = 0x90200+gdt

msg1:
    .byte 13, 10
    .asciz "Setup Loading..."
