#include <linux/sched.h>
#include <linux/tty.h>

/**
 * 每个终端三个缓冲队列
 *  read_queue: 缓冲键盘或串行输入
 *  write_queue: 缓冲屏幕或串行输出
 *  secondary: 保存规范模式字符的辅助缓冲队列
 **/
#define QUEUES	(3*(MAX_CONSOLES+NR_SERIALS+2*NR_PTYS))
static struct tty_queue tty_queues[QUEUES];
struct tty_struct tty_table[256];       // tty表终端结构数组，缓冲

/**
 * 所有终端的缓冲区都放在tty_queues结构里
 *  8个虚拟控制台终端占用开头24项
 *  2个串行终端占用随后的6项
 *  4个主伪终端占用随后的12项
 *  4个从伪终端占用随后的12项
 **/
#define con_queues tty_queues
#define rs_queues ((3*MAX_CONSOLES) + tty_queues)
#define mpty_queues ((3*(MAX_CONSOLES+NR_SERIALS)) + tty_queues)
#define spty_queues ((3*(MAX_CONSOLES+NR_SERIALS+NR_PTYS)) + tty_queues)

/**
 * 各种类型终端使用的tty结构都存放在tty_table中
 *  8个虚拟控制台终端占用开头64项（0 - 63）
 *  2个串行终端占用随后的2项（64 - 65）
 *  4个主伪终端占用从128开始的随后64项（128 - 191）
 *  4个从伪终端占用从192开始的随后64项（192 - 255）
 **/
#define con_table tty_table
#define rs_table (64+tty_table)
#define mpty_table (128+tty_table)
#define spty_table (192+tty_table)

int fg_console = 0;

void chr_dev_init(void) {}

void tty_init(void) {
    int i;

    for (i = 0; i < QUEUES; i++) {
        tty_queues[i] = (struct tty_queue) {0, 0, 0, 0, ""};
    }
    /**
     * 串行终端的读写队列，其data字段设置为串行端口基地址
     *  串口1：0x3f8
     *  串口2：0x2f8
     **/
    rs_queues[0] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
    rs_queues[1] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
    rs_queues[3] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
    rs_queues[4] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};

    for (i = 0; i < 256; i++) {
        tty_table[i] = (struct tty_struct) {
            {0, 0, 0, 0, 0, INIT_C_CC},
            0, 0, 0, NULL, NULL, NULL, NULL
        };
    }
    con_init();
}