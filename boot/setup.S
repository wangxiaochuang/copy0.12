#include <linux/config.h>
#include <linux/segment.h>

#ifndef SVGA_MODE
#define SVGA_MODE ASK_VGA
#endif

INITSEG  = DEF_INITSEG
SYSSEG   = DEF_SYSSEG
SETUPSEG = DEF_SETUPSEG

.code16

entry:
    mov $INITSEG, %ax
    mov %ax, %ds

    // 获取从1M开始的内存，kb为单位，然后放到0x90002处
    mov $0x88, %ah
    int $0x15
    mov %ax, 2          

    // 设置键盘repeat rate到最大值
    mov $0x0305, %ax
    xor %bx, %bx
    int $0x16

    // 检测显示方式（EGA/VGA）并获取参数
    mov $0x12, %ah
    mov $0x10, %bl
    int $0x10
    mov %ax, 8
    mov %bx, 10     // 安装的显存与显示模式
    mov %cx, 12     // 显示卡特性参数
    mov $0x5019, %ax    // col: 80, raw: 25
    cmp $0x10, %bl
    je novga        // bl即显示模式，如果等于0x10，表示不是VGA
    mov $0x1a00, %ax    // 区分到底是EGA还是VGA
    int $0x10
    mov %ax, %bx
    mov $0x5019, %ax
    cmp $0x1a, %bl
    jne novga       // 如果是1a，则是VGA，否则就是EGA或其他的，跳转（是）
    call chsvga     // 是VGA，进一步确认其他参数
novga:
    mov %ax, 14     // 保存分辨率行列值
    mov $0x03, %ah
    xor %bh, %bh
    int $0x10
    mov %dx, 0      // 保存当前光标位置

    mov $0x0f, %ah
    int $0x10
    mov %bx, 4      // 显示页面
    mov %ax, 6      // al: video mode, ah: window width

// hard disk 0 param
    xor %ax, %ax
    mov %ax, %ds
    lds 0x41*4, %si
    mov $INITSEG, %ax
    mov %ax, %es
    mov $0x0080, %di
    mov $0x10, %cx  // 参数表16个字节
    cld
    rep movsb       // ds:si => es:di
// hard disk 1 param
    xor %ax, %ax
    mov %ax, %ds
    lds 0x46*4, %si
    mov $INITSEG, %ax
    mov %ax, %es
    mov $0x0090, %di
    mov $0x10, %cx  // 参数表16个字节
    cld
    rep movsb       // ds:si => es:di

// 检查是否存在hd1
    mov $0x1500, %ax    // ah=0x15，功能号，取盘类型
    mov $0x81, %dl      // 0x8x是硬盘，0x80是第一块硬盘，0x81是第二块硬盘
    int $0x13
    jc no_disk1         // 不存在这个盘，CF置位
    cmp $3, %ah         // ah: 01,软驱，没有change-line支持；02,软驱或其他，有change-line支持，03,硬盘
    je is_disk1
no_disk1:
    mov $INITSEG, %ax
    mov %ax, %es
    mov $0x0090, %di
    mov $0x10, %cx
    xor %ax, %ax
    cld
    rep stosb
is_disk1:
// 检查PS/2 鼠标设备
    mov $INITSEG, %ax
    mov %ax, %ds
    movb $0, 0x1ff       // 0x9001ff 最后一个字节，默认表示不存在
    int $0x11
    test $0x04, %al
    jz no_psmouse
    movb $0xaa, 0x1ff
no_psmouse:
    cli
    mov $0x80, %al      // disable NMI for bootup sequence
    out %al, $0x70      // 0x80这个值写入0x70端口

// mov system to memory 0x100:0
    mov $0x100, %ax     // 目的段0x100
    mov $0x1000, %bx    // 源段0x1000
    cld
do_move:
    mov %ax, %es
    add $0x100, %ax
    cmp $0x9000, %ax
    jz end_move
    mov %bx, %ds
    add $0x100, %bx
    xor %di, %di
    xor %si, %si
    mov $0x800, %cx     // 0x800 * 2 = 0x1000
    rep movsw           // ds:si => es:di
    jmp do_move
end_move:
    mov $SETUPSEG, %ax
    mov %ax, %ds
    lidt idt_48
    lgdt gdt_48
// enable A20
    call empty_8842
    mov $0xd1, %al
    out %al, $0x64
    call empty_8842
    mov $0xdf, %al
    out %al, $0x60
    call empty_8842
// make sure any possible coprocessor is properly reset
    xor %ax, %ax
    out %al, $0xf0
    call delay
    out %al, $0xf1
    call delay

    // 8259A 重编程
    mov $0x11, %al
    out %al, $0x20
    call delay
    out %al, $0xa0
    call delay
    mov $0x20, %al
    out %al, $0x21
    call delay
    mov $0x28, %al
    out %al, $0xa1
    call delay
    mov $0x04, %al
    out %al, $0x21
    call delay
    mov $0x02, %al
    out %al, $0xa1
    call delay
    mov $0x01, %al
    out %al, $0x21
    call delay
    out %al, $0xa1
    call delay
    mov $0xff, %al
    out %al, $0xa1
    call delay
    mov $0xfb, %al
    out %al, $0x21


    mov $0x0001, %ax
    lmsw %ax
    // movl %cr0, %eax
    // or $1, %eax
    // movl %eax, %cr0
    jmp flush_instr
flush_instr:
    jmp $KERNEL_CS, $0x1000

empty_8842:             // 测试8042状态寄存器，等待输入缓冲器空
    call delay
    inb $0x64, %al      // 从0x64端口读一个字节到al
    testb $1, %al       // output buffer 是否为空
    jz no_output
    call delay
    inb $0x60, %al
    jmp empty_8842
no_output:
    testb $2, %al       // is input buffer full?
    jnz empty_8842
    ret


flush:
    mov $0x01, %ah
    int $0x16
    jz empty
    xor %ah, %ah
    int $0x16
    jmp flush
empty:
    ret

delay:
    .word 0x00eb    // jmp $+2
    ret

chsvga:
    cld
    push %ds
    push %cs
    mov 0x01fa, %ax     // 引导扇区最后几个字节，该位置记录了SVGA模式
    pop %ds
    mov %ax, modesave
    mov $0xc000, %ax
    mov %ax, %es
    mov modesave, %ax
    cmp $NORMAL_VGA, %ax
    je defvga
    cmp $EXTENDED_VGA, %ax
    je vga50
    cmp $ASK_VGA, %ax
    jne svga
    lea msg1, %si
    call prtstr
    call flush

defvga:
    mov $0x5019, %ax
    pop %ds
    ret
vga50:
svga:

prtstr:
    lodsb
    and %al, %al
    jz fin
    call prnt1
    jmp prtstr
fin:
    ret

prnt1:
    push %ax
    push %cx
    xor %bh, %bh
    mov $0x01, %cx
    mov $0x0e, %ah
    int $0x10
    pop %cx
    pop %ax
    ret

.align 8
gdt:
	.word	0,0,0,0
	.word	0,0,0,0

	.word	0x07FF
	.word	0x0000
	.word	0x9A00
	.word	0x00C0

	.word	0x07FF
	.word	0x0000
	.word	0x9200
	.word	0x00C0

idt_48:
    .word 0
    .word 0, 0

gdt_48:
    .word 0x800
    .word gdt+512, 0x9      // gdt base: 0x90200+gdt

msg1:	
    .ascii	"Press <RETURN> to see SVGA-modes available, <SPACE> to continue or wait 30 secs."
	.byte	0x0d, 0x0a, 0x0a, 0x00


modesave:
    .word	SVGA_MODE