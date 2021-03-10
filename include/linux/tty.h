#ifndef _TTY_H
#define _TTY_H

#define MAX_CONSOLES	8
#define NR_SERIALS	4
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

#define IS_A_CONSOLE(min)	(((min) & 0xC0) == 0x00)
#define IS_A_SERIAL(min)	(((min) & 0xC0) == 0x40)
#define IS_A_PTY(min)		((min) & 0x80)
#define IS_A_PTY_MASTER(min)	(((min) & 0xC0) == 0x80)
#define IS_A_PTY_SLAVE(min)	(((min) & 0xC0) == 0xC0)
#define PTY_OTHER(min)		((min) ^ 0x40)

#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
#define EMPTY(a) ((a)->head == (a)->tail)
#define LEFT(a) (((a)->tail-(a)->head-1)&(TTY_BUF_SIZE-1))
#define LAST(a) ((a)->buf[(TTY_BUF_SIZE-1)&((a)->head-1)])
#define FULL(a) (!LEFT(a))
#define CHARS(a) (((a)->head-(a)->tail)&(TTY_BUF_SIZE-1))
#define GETCH(queue,c) \
(void)({c=(queue)->buf[(queue)->tail];INC((queue)->tail);})
#define PUTCH(c,queue) \
(void)({(queue)->buf[(queue)->head]=(c);INC((queue)->head);})

#define INTR_CHAR(tty)		((tty)->termios.c_cc[VINTR])
#define QUIT_CHAR(tty)		((tty)->termios.c_cc[VQUIT])
#define ERASE_CHAR(tty)		((tty)->termios.c_cc[VERASE])
#define KILL_CHAR(tty)		((tty)->termios.c_cc[VKILL])
#define EOF_CHAR(tty)		((tty)->termios.c_cc[VEOF])
#define START_CHAR(tty)		((tty)->termios.c_cc[VSTART])
#define STOP_CHAR(tty)		((tty)->termios.c_cc[VSTOP])
#define SUSPEND_CHAR(tty)	((tty)->termios.c_cc[VSUSP])

/* tty数据结构 */
struct tty_struct {
	struct termios termios;		/* 终端io属性和控制字符数据结构 */
	int pgrp;					/* 所属进程组 */
	int session;				/* 会话号 */
	int stopped;				/* 停止标志 */
	int busy;
	struct winsize winsize;
	void (*write)(struct tty_struct * tty);	/* tty写函数指针 */
	struct tty_queue *read_q;	/* tty读队列 */
	struct tty_queue *write_q;	/* tty写队列 */
	struct tty_queue *secondary;/* tty辅助队列(存放规范模式字符序列) */
};

#define TTY_WRITE_BUSY 1
#define TTY_READ_BUSY 2

#define TTY_WRITE_FLUSH(tty) \
do { \
	cli(); \
	if (!EMPTY((tty)->write_q) && !(TTY_WRITE_BUSY & (tty)->busy)) { \
		(tty)->busy |= TTY_WRITE_BUSY; \
		sti(); \
		(tty)->write((tty)); \
		cli(); \
		(tty)->busy &= ~TTY_WRITE_BUSY; \
	} \
	sti(); \
} while (0)

#define TTY_READ_FLUSH(tty) \
do { \
	cli(); \
	if (!EMPTY((tty)->read_q) && !(TTY_READ_BUSY & (tty)->busy)) { \
		(tty)->busy |= TTY_READ_BUSY; \
		sti(); \
		copy_to_cooked((tty)); \
		cli(); \
		(tty)->busy &= ~TTY_READ_BUSY;
	} \
	sti(); \
} while (0)

extern struct tty_struct tty_table[];
extern int fg_console;
extern unsigned long video_num_columns;
extern unsigned long video_num_lines;

#define TTY_TABLE(nr) \
(tty_table + ((nr) ? (((nr) < 64)?(nr)-1:(nr)) : fg_console))

#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

void rs_init(void);
void con_init(void);
void tty_init(void);

int tty_read(unsigned c, char * buf, int n);
int tty_write(unsigned c, char * buf, int n);

void con_write(struct tty_struct * tty);
void rs_write(struct tty_struct * tty);
void mpty_write(struct tty_struct * tty);
void spty_write(struct tty_struct * tty);

extern void serial_open(unsigned int line);

void copy_to_cooked(struct tty_struct * tty);

void update_screen(void);

#endif