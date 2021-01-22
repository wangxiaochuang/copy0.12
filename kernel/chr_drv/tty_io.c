#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/segment.h>
#include <asm/system.h>

int kill_pg(int pgrp, int sig, int priv);
/* 判断一个进程组是否是孤儿进程 */
int is_orphaned_pgrp(int pgrp);

#define _L_FLAG(tty,f)  ((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)  ((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)  ((tty)->termios.c_oflag & f)

#define L_CANON(tty)    _L_FLAG((tty),ICANON)
#define L_ISIG(tty)     _L_FLAG((tty),ISIG)
#define L_ECHO(tty)     _L_FLAG((tty),ECHO)
#define L_ECHOE(tty)    _L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)    _L_FLAG((tty),ECHOK)
#define L_ECHOCTL(tty)  _L_FLAG((tty),ECHOCTL)
#define L_ECHOKE(tty)   _L_FLAG((tty),ECHOKE)
#define L_TOSTOP(tty)   _L_FLAG((tty),TOSTOP)

#define I_UCLC(tty)     _I_FLAG((tty),IUCLC)
#define I_NLCR(tty)     _I_FLAG((tty),INLCR)
#define I_CRNL(tty)     _I_FLAG((tty),ICRNL)
#define I_NOCR(tty)     _I_FLAG((tty),IGNCR)
#define I_IXON(tty)     _I_FLAG((tty),IXON)

#define O_POST(tty)	_O_FLAG((tty),OPOST)
#define O_NLCR(tty)	_O_FLAG((tty),ONLCR)
#define O_CRNL(tty)	_O_FLAG((tty),OCRNL)
#define O_NLRET(tty)	_O_FLAG((tty),ONLRET)
#define O_LCUC(tty)	_O_FLAG((tty),OLCUC)

#define C_SPEED(tty)	((tty)->termios.c_cflag & CBAUD)
#define C_HUP(tty)	(C_SPEED((tty)) == B0)

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

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

static void sleep_if_empty(struct tty_queue * queue) {
	cli();
	while (!(current->signal & ~current->blocked) && EMPTY(queue))
		interruptible_sleep_on(&queue->proc_list);
	sti();
}
static void sleep_if_full(struct tty_queue * queue) {
	if (!FULL(queue))
		return;
	cli();
	while (!(current->signal & ~current->blocked) && LEFT(queue)<128)
		interruptible_sleep_on(&queue->proc_list);
	sti();
}

void copy_to_cooked(struct tty_struct * tty)
{
	signed char c;

	if (!(tty->read_q || tty->write_q || tty->secondary)) {
		printk("copy_to_cooked: missing queues\n\r");
		return;
	}
	while (1) {
		if (EMPTY(tty->read_q))
			break;
		if (FULL(tty->secondary))
			break;
		GETCH(tty->read_q,c);
		if (c==13) {
			if (I_CRNL(tty))
				c=10;
			else if (I_NOCR(tty))
				continue;
		} else if (c==10 && I_NLCR(tty))
			c=13;
		if (I_UCLC(tty))
			c = tolower(c);
		if (L_CANON(tty)) {
			if ((KILL_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==KILL_CHAR(tty))) {
				/* deal with killing the input line */
				while(!(EMPTY(tty->secondary) ||
				        (c=LAST(tty->secondary))==10 ||
				        ((EOF_CHAR(tty) != _POSIX_VDISABLE) &&
					 (c==EOF_CHAR(tty))))) {
					if (L_ECHO(tty)) {
						if (c<32)
							PUTCH(127,tty->write_q);
						PUTCH(127,tty->write_q);
						tty->write(tty);
					}
					DEC(tty->secondary->head);
				}
				continue;
			}
			if ((ERASE_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==ERASE_CHAR(tty))) {
				if (EMPTY(tty->secondary) ||
				   (c=LAST(tty->secondary))==10 ||
				   ((EOF_CHAR(tty) != _POSIX_VDISABLE) &&
				    (c==EOF_CHAR(tty))))
					continue;
				if (L_ECHO(tty)) {
					if (c<32)
						PUTCH(127,tty->write_q);
					PUTCH(127,tty->write_q);
					tty->write(tty);
				}
				DEC(tty->secondary->head);
				continue;
			}
		}
		if (I_IXON(tty)) {
			if ((STOP_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==STOP_CHAR(tty))) {
				tty->stopped=1;
				tty->write(tty);
				continue;
			}
			if ((START_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==START_CHAR(tty))) {
				tty->stopped=0;
				tty->write(tty);
				continue;
			}
		}
		if (L_ISIG(tty)) {
			if ((INTR_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==INTR_CHAR(tty))) {
				kill_pg(tty->pgrp, SIGINT, 1);
				continue;
			}
			if ((QUIT_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==QUIT_CHAR(tty))) {
				kill_pg(tty->pgrp, SIGQUIT, 1);
				continue;
			}
			if ((SUSPEND_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==SUSPEND_CHAR(tty))) {
				if (!is_orphaned_pgrp(tty->pgrp))
					kill_pg(tty->pgrp, SIGTSTP, 1);
				continue;
			}
		}
		if (c==10 || (EOF_CHAR(tty) != _POSIX_VDISABLE &&
			      c==EOF_CHAR(tty)))
			tty->secondary->data++;
		if (L_ECHO(tty)) {
			if (c==10) {
				PUTCH(10,tty->write_q);
				PUTCH(13,tty->write_q);
			} else if (c<32) {
				if (L_ECHOCTL(tty)) {
					PUTCH('^',tty->write_q);
					PUTCH(c+64,tty->write_q);
				}
			} else
				PUTCH(c,tty->write_q);
			tty->write(tty);
		}
		PUTCH(c,tty->secondary);
	}
	wake_up(&tty->secondary->proc_list);
}

void chr_dev_init(void) {}

int tty_read(unsigned channel, char * buf, int nr) {
    return -1;
}

int tty_write(unsigned channel, char *buf, int nr) {
    static int cr_flag = 0;
    struct tty_struct *tty;
    char c, *b = buf;

    if (channel > 255)
        return -EIO;
    tty = TTY_TABLE(channel);
    if (!(tty->read_q || tty->write_q || tty->secondary))
        return -EIO;
    // @todo
    if (L_TOSTOP(tty) && (current->tty == channel) && (tty->pgrp != current->pgrp)) 
        panic("need return tty_signal");
		//return(tty_signal(SIGTTIN, tty));
    while (nr > 0) {
        sleep_if_full(tty->write_q);
        if (current->signal & ~current->blocked)
			break;
        while (nr > 0 && !FULL(tty->write_q)) {
            c = get_fs_byte(b);
            if (O_POST(tty)) {
                if (c == '\r' && O_CRNL(tty))
					c = '\n';
				else if (c == '\n' && O_NLRET(tty))
					c = '\r';
				if (c == '\n' && !cr_flag && O_NLCR(tty)) {
					cr_flag = 1;
					PUTCH(13,tty->write_q);
					continue;
				}
				if (O_LCUC(tty))
					c=toupper(c);
            }
            b++; nr--;
            cr_flag = 0;
            PUTCH(c,tty->write_q);
        }
        tty->write(tty);
        if (nr > 0)
            schedule();
    }
    return (b - buf);
}

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
    for (i = 0; i < NR_CONSOLES; i++) {
        con_table[i] = (struct tty_struct) {
            {ICRNL,
            OPOST|ONLCR,
            0,
            IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
            0,
            INIT_C_CC},
            0,
            0,
            0,
            con_write,
            con_queues + i*3 + 0,
            con_queues + i*3 + 1,
            con_queues + i*3 + 2,
        };
    }
}