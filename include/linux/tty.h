#ifndef _TTY_H
#define _TTY_H

#define MAX_CONSOLES	8
#define NR_SERIALS	2
#define NR_PTYS		4

extern int NR_CONSOLES;

#include <termios.h>

#define TTY_BUF_SIZE 1024

struct tty_queue {
	unsigned long data;
	unsigned long head;
	unsigned long tail;
	struct task_struct *proc_list;
	char buf[TTY_BUF_SIZE];
};

#define IS_A_PTY(min)		((min) & 0x80)
#define IS_A_PTY_MASTER(min)	(((min) & 0xC0) == 0x80)
#define IS_A_PTY_SLAVE(min)	(((min) & 0xC0) == 0xC0)

/* tty数据结构 */
struct tty_struct {
	struct termios termios;		/* 终端io属性和控制字符数据结构 */
	int pgrp;					/* 所属进程组 */
	int session;				/* 会话号 */
	int stopped;				/* 停止标志 */
	void (*write)(struct tty_struct * tty);	/* tty写函数指针 */
	struct tty_queue *read_q;	/* tty读队列 */
	struct tty_queue *write_q;	/* tty写队列 */
	struct tty_queue *secondary;/* tty辅助队列(存放规范模式字符序列) */
};

extern struct tty_struct tty_table[];
extern int fg_console;

#define TTY_TABLE(nr) \
(tty_table + ((nr) ? (((nr) < 64)?(nr)-1:(nr)) : fg_console))

#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void con_init(void);
void tty_init(void);
void update_screen(void);

#endif