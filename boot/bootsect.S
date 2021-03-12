#include <linux/config.h>
SYSSIZE = DEF_SYSSIZE

.text
.code16

SETUPSECS   = 4
BOOTSEG     = 0x07c0
INITSEG     = DEF_INITSEG
SETUPSEG    = DEF_SETUPSEG
SYSSEG      = DEF_SYSSEG

ROOT_DEV = 0
SWAP_DEV = 0

#ifndef SVGA_MODE
#define SVGA_MODE ASK_VGA
#endif

#ifndef RAMDISK
#define RAMDISK 0
#endif 

#ifndef CONFIG_ROOT_RDONLY
#define CONFIG_ROOT_RDONLY 0
#endif

entry:
    mov $BOOTSEG, %ax
    mov %ax, %ds
    mov $INITSEG, %ax
    mov %ax, %es
    mov $256, %cx
    xor %si, %si
    xor %di, %di
    cld
    rep movsw           // mov 512B from 0x7c00 to 0x90000
    jmp $INITSEG, $go

go:
    mov %cs, %ax
    mov $0x4000-12, %dx
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %dx, %sp        // top of stack: 0x9000:(0x4000-12)

    push $0
    pop %fs
    mov $0x78, %bx
    fs
    lgs (%bx), %si      // fs:bx => gs:si
    mov %dx, %di
    mov $6, %cx         // copy 12B
    cld

    rep
    gs
    movsw               // gs:si => es:di  0x9000:0x3ff4

    mov %dx, %di
    movb $18, 4(%di)    // 修改软驱参数表，最大读取块为18（bochs里默认就是18）

    fs
    mov %di, (%bx)      // di => fs:0x78
    fs
    mov %es, 2(%bx)     // 0x78偏移2的位置为参数表的段寄存器

    mov %cs, %ax
    mov %ax, %fs
    mov %ax, %gs

    xor %ah, %ah        // 复位软盘控制器，让其采用新参数
    xor %dl, %dl        // dl = 0，表示第一个软驱
    int $0x13

load_setup:
    xor %dx, %dx
    mov $0x0002, %cx                // ch: 磁道号的低8位，cl: 开始扇区（位0-5），磁道号高2位（位6-7）磁道号为0
    mov $0x0200, %bx                // es:bx => 指向数据缓冲区0x90200，如果出错，CF标志置位，ah中是出错码
    mov $0x0200+SETUPSECS, %ax      // ah: 读磁盘扇区到内存 al: 需要读出的扇区数量
    int $0x13
    jnc ok_load_setup

ok_load_setup:
    xor %dl, %dl
    mov $0x08, %ah
    int $0x13           // ah: 0x08功能号，dl: 驱动器号，如果是硬盘要置位7为1
                        // ch: 最大磁道号的低8位，cl: 每磁道最大扇区数（位0-5），最大磁道号高2位（位6-7）
                        // dh: 最大磁头数，dl: 驱动器数量
    xor %ch, %ch        // 最大磁道号清了，只想记录每磁道的扇区数，最大磁道肯定不会超过256，所以cx现在就是表示每磁道最大扇区数

got_sectors:
    cs
    mov %cx, sectors
    mov $INITSEG, %ax
    mov %ax, %es        // 获取磁盘参数或修改es的值

    mov $0x03, %ah
    xor %bh, %bh
    int $0x10           // 获得dh行号，dl列号，为下面做准备

    mov $9, %cx         // 显示9个字符
    mov $0x0007, %bx    // page 0, attr 7 (normal)
    mov $msg1, %bp      // es:bp 指向要显示的字符串
    mov $0x1301, %ax    // write string, move cursor，al为1表示使用bl的属性值，光标停在字符串结尾处
    int $0x10

    mov $SYSSEG, %ax
    mov %ax, %es
    call read_it
    call kill_motor
    call print_nl

    cs
    mov root_dev, %ax
    or %ax, %ax
    jne root_defined
    cs
    mov sectors, %bx
    mov $0x0208, %ax        // 1.2MB
    cmp %bx, 15
    je root_defined
    mov $0x021c, %ax        // 1.44MB
    cmp %bx, 18
    je root_defined
    mov $0x0200, %ax        // autodetect

root_defined:
    cs
    jmp .
    mov %ax, root_dev
    jmp $SETUPSEG, $0

sread:
    .word 1+SETUPSECS
head:
    .word 0
track:
    .word 0

read_it:
    mov %es, %ax        // es -> 0x1000 inc 0x1000
    test $0x0fff, %ax
die:
    jne die             // es must be at 64KB boundary
    xor %bx, %bx
rp_read:
    mov %es, %ax
    sub $SYSSEG, %ax    // es - 0x1000 = alread read
    cmp %ax, syssize
    jbe ok1_read        // 每次es会加0x1000，ax - SYSSEG就是已经读取数量，如果小于syssize，说明还要读
    ret
ok1_read:
    cs
    mov sectors, %ax
    sub sread, %ax      // 每磁道扇区数减去已经读的该磁道扇区，就是需要读的，供read_track使用
    mov %ax, %cx        // 当前磁道剩余的扇区数
    shl $9, %cx         // cx * 512
    add %bx, %cx        // 此次读操作后，段内共读入的字节数
    jnc ok2_read        // 没有超过64KB，跳转
    je ok2_read
    xor %ax, %ax
    sub %bx, %ax        // 0减去某数就是概述64KB的补值
    shr $9, %ax         // 需要读的扇区数，read_track里用
ok2_read:
    call read_track
    mov %ax, %cx
    add sread, %ax      // 增加已读的记录
    cs
    cmp sectors, %ax    // 看看是否超过最大扇区数
    jne ok3_read
    mov $1, %ax         
    sub head, %ax       // 判断当前磁头号
    jne ok4_read        // 如果是0磁头，则再取读1磁头面的扇区数据
    incw track          // 否则读下一磁道
ok4_read:
    mov %ax, head
    xor %ax, %ax
ok3_read:
    mov %ax, sread
    shl $9, %cx
    add %cx, %bx
    jnc rp_read
    mov %es, %ax
    add $0x10, %ah
    mov %ax, %es
    xor %bx, %bx
    jmp rp_read

read_track:
    pusha
    pusha
    mov $0x0e2e, %ax    // loading... message 2e = .
    mov $7, %bx
    int $0x10
    popa

    mov track, %dx      // 当前磁道号
    mov sread, %cx      // 已经读的扇区数
    inc %cx             // 下一个要读的起始扇区
    mov %dl, %ch        // ch: 当前磁道号
    mov head, %dx           
    mov %dl, %dh        // dh: 当前磁头号，dl: 驱动器号（0表示当前A驱动器）
    and $0x0100, %dx    // 磁头号不会大于1，驱动器号是0，所以与操作
    mov $2, %ah         // 读磁盘扇区功能号

    push %dx
    push %cx
    push %bx
    push %ax

    int $0x13
    jc bad_rt           // 出错会修改进位标志，跳转到bad_rt
    add $8, %sp
    popa
    ret

bad_rt:
    push %ax
    call print_all

    xor %ah, %ah
    xor %dl, %dl

print_nl:
    mov $0x0e0d, %ax
    int $0x10
    mov $0x0a, %al
    int $0x10
    ret

kill_motor:
    push %dx
    mov $0x03f2, %dx
    xor %al, %al
    outb %al, %dx
    pop %dx
    ret

sectors:
	.word 0

msg1:
	.byte 13, 10
	.ascii "Loading"

.org 498
root_flags:
	.word CONFIG_ROOT_RDONLY
syssize:
	.word SYSSIZE
swap_dev:
	.word SWAP_DEV
ram_size:
	.word RAMDISK
vid_mode:
	.word SVGA_MODE
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55