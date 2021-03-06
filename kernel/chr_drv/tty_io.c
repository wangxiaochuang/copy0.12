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
#define I_STRP(tty)		_I_FLAG((tty),ISTRIP)

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
 *  4个串行终端占用随后的12项
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
 *  4个串行终端占用随后的4项（64 - 67）
 *  4个主伪终端占用从128开始的随后64项（128 - 191）
 *  4个从伪终端占用从192开始的随后64项（192 - 255）
 **/
#define con_table tty_table
#define rs_table (64+tty_table)
#define mpty_table (128+tty_table)
#define spty_table (192+tty_table)

int fg_console = 0;

struct tty_queue * table_list[] = {
	con_queues + 0, con_queues + 1,
	rs_queues + 0, rs_queues + 1,
	rs_queues + 3, rs_queues + 4
};

void change_console(unsigned int new_console) {
	if (new_console == fg_console || new_console >= NR_CONSOLES)
		return;
	fg_console = new_console;
	table_list[0] = con_queues + 0 + fg_console * 3;
	table_list[1] = con_queues + 1 + fg_console * 3;
	update_screen();
}

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

void copy_to_cooked(struct tty_struct * tty) {
	unsigned char c;

	if (!(tty && tty->write && tty->read_q &&
	    tty->write_q && tty->secondary)) {
		printk("copy_to_cooked: missing queues\n\r");
		return;
	}
	while (1) {
		if (EMPTY(tty->read_q))
			break;
		if (FULL(tty->secondary)) {
			if (tty->secondary->proc_list)
				if (tty->secondary->proc_list != current)
					current->counter = 0;
			break;
		}
		GETCH(tty->read_q,c);
		if (I_STRP(tty))
			c &= 0x7f;
		if (c==13) {
			if (I_CRNL(tty))
				c=10;
			else if (I_NOCR(tty))
				continue;
		} else if (c==10 && I_NLCR(tty))
			c=13;
		if (I_UCLC(tty))
			c=tolower(c);
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
						TTY_WRITE_FLUSH(tty);
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
					TTY_WRITE_FLUSH(tty);
				}
				DEC(tty->secondary->head);
				continue;
			}
		}
		if (I_IXON(tty)) {
			if ((STOP_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==STOP_CHAR(tty))) {
				tty->stopped=1;
				continue;
			}
			if ((START_CHAR(tty) != _POSIX_VDISABLE) &&
			    (c==START_CHAR(tty))) {
				tty->stopped=0;
				TTY_WRITE_FLUSH(tty);
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
		if ((L_ECHO(tty) || L_ECHONL(tty)) && (c==10)) {
			PUTCH(10,tty->write_q);
			PUTCH(13,tty->write_q);
		} else if (L_ECHO(tty)) {
			if (c<32 && L_ECHOCTL(tty)) {
				PUTCH('^',tty->write_q);
				PUTCH(c+64,tty->write_q);
			} else
				PUTCH(c,tty->write_q);
		}
		PUTCH(c,tty->secondary);
		TTY_WRITE_FLUSH(tty);
	}
	TTY_WRITE_FLUSH(tty);
	if (!EMPTY(tty->secondary))
		wake_up(&tty->secondary->proc_list);
	if (LEFT(tty->write_q) > TTY_BUF_SIZE/2)
		wake_up(&tty->write_q->proc_list);
}

void do_tty_interrupt(int tty) {
	copy_to_cooked(TTY_TABLE(tty));
}

void chr_dev_init(void) {}

// 用于向后台进程组中的所有进程发送SIGTTIN或SIGTTOU信号
// 无论后台进程组的进程是否已经阻塞或忽略了信号，当前进程都将立刻退出读写操作而返回
int tty_signal(int sig, struct tty_struct *tty) {
	// 不能停止一个孤儿进程组中的进程
	if (is_orphaned_pgrp(current->pgrp))
		return -EIO;
	(void) kill_pg(current->pgrp, sig, 1);
	if ((current->blocked & (1 << (sig - 1))) || 
		((int) current->sigaction[sig - 1].sa_handler == 1))
		return -EIO;
	else if (current->sigaction[sig-1].sa_handler)
		return -EINTR;
	else
		return -ERESTARTSYS;
}

int tty_read(unsigned channel, char * buf, int nr) {
	struct tty_struct *tty;
	struct tty_struct *other_tty = NULL;
	char c, *b = buf;
	int minimum, time;

	if (channel > 255) return -EIO;
	tty = TTY_TABLE(channel);
	if (!(tty->write_q || tty->read_q || tty->secondary)) return -EIO;
	if ((current->tty == channel) && (tty->pgrp != current->pgrp)) {
		return (tty_signal(SIGTTIN, tty));
	}
	// 如果channel大于等于128，则表示是伪终端，那么对应的另一个伪终端就是other_tty
	if (channel & 0x80)
		other_tty = tty_table + (channel ^ 0x40);
	// 读操作超时定时值
	time = 10L * tty->termios.c_cc[VTIME];
	// 最少需要读取的字符个数
	minimum = tty->termios.c_cc[VMIN];
	// 如果处于规范模式，则最少读取字符数为进程欲读字符数nr，且不超时
	if (L_CANON(tty)) {
		minimum = nr;
		current->timeout = 0xffffffff;
		time = 0;
	// 非规范模式，且设置了最少读取字符数，则临时设置不超时，以让进程先读取辅助队列中已有字符
	} else if (minimum) {
		current->timeout = 0xffffffff;
	// 非规范模式，且没有设置最少读取字符数，如果设置了超时定时值time，则设置current->timeout
	} else {
		minimum = nr;
		if (time)
			current->timeout = time + jiffies;
		time = 0;
	}
	// 如果设置的最少读取字符数大于程序要读的，就只读程序要读的数
	// 即：规范模式下的读取操作不收VTIME、VMIN约束，他们仅在非规范模式中起作用
	if (minimum > nr)
		minimum = nr;
	while (nr > 0) {
		// 如果是要读的是伪终端，就让其配对的伪终端写，这样其写到本终端的读队列经过行规则函数转换后放入辅助队列
		if (other_tty)
			other_tty->write(other_tty);
		cli();
		// 辅助队列为空或者设置了规范模式且读队列未满且辅助队列字符行数为0
		if (EMPTY(tty->secondary) || (L_CANON(tty) && 
			!FULL(tty->read_q) && !tty->secondary->data)) {
            // 没有设置过读字符超时值或者收到信号则退出
            if (!current->timeout || 
				(current->signal & ~current->blocked)) {
				sti();
				break;
			}
			// 本终端是从伪终端且对应主伪终端已经挂断，则直接退出
			if (IS_A_PTY_SLAVE(channel) && C_HUP(other_tty))
				break;
			// 进入可中断睡眠，等待读入
			interruptible_sleep_on(&tty->secondary->proc_list);
			sti();
			continue;
		}
		sti();
		do {
			GETCH(tty->secondary, c);
			if ((EOF_CHAR(tty) != _POSIX_VDISABLE && 
				c == EOF_CHAR(tty)) || c == 10)
				tty->secondary->data--;
			if ((EOF_CHAR(tty) != _POSIX_VDISABLE && 
				c == EOF_CHAR(tty)) && L_CANON(tty))
				break;
			else {
				put_fs_byte(c, b++);
				if (!--nr)
					break;
			}
			if (c == 10 && L_CANON(tty))
				break;
		} while (nr > 0 && !EMPTY(tty->secondary));
		wake_up(&tty->read_q->proc_list);
		if (time)
			current->timeout = time + jiffies;
		if (L_CANON(tty) || b - buf >= minimum)
			break;
	}
	// 复位读取超时定时值，如果收到信号且还没读取任何字符，则重启系统调用
	current->timeout = 0;
	if ((current->signal & ~current->blocked) && !(b - buf))
		return -ERESTARTSYS;
	return (b - buf);
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
    if (L_TOSTOP(tty) && 
		(current->tty == channel) && (tty->pgrp != current->pgrp)) 
		return(tty_signal(SIGTTOU, tty));
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

    for (i = 0; i < QUEUES; i++)
        tty_queues[i] = (struct tty_queue) {0, 0, 0, 0, ""};
    /**
     * 串行终端的读写队列，其data字段设置为串行端口基地址
     *  串口1：0x3f8
     *  串口2：0x2f8
     **/
    rs_queues[0] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
    rs_queues[1] = (struct tty_queue) {0x3f8, 0, 0, 0, ""};
    rs_queues[3] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
    rs_queues[4] = (struct tty_queue) {0x2f8, 0, 0, 0, ""};
    rs_queues[6] = (struct tty_queue) {0x3e8, 0, 0, 0, ""};
    rs_queues[7] = (struct tty_queue) {0x3e8, 0, 0, 0, ""};
    rs_queues[9] = (struct tty_queue) {0x2e8, 0, 0, 0, ""};
    rs_queues[10] = (struct tty_queue) {0x2e8, 0, 0, 0, ""};

    for (i = 0; i < 256; i++) {
        tty_table[i] = (struct tty_struct) {
            {0, 0, 0, 0, 0, INIT_C_CC},
            -1, 0, 0, 0, {0, 0, 0, 0},
			NULL, NULL, NULL, NULL
        };
    }
    con_init();
    for (i = 0; i < NR_CONSOLES; i++) {
        con_table[i] = (struct tty_struct) {
            {ICRNL,
            OPOST|ONLCR,
			B38400 | CS8,
			IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
            0,
            INIT_C_CC},
			-1,
            0,
            0,
            0,
			{video_num_lines, video_num_columns, 0, 0},
            con_write,
            con_queues + i*3 + 0,
            con_queues + i*3 + 1,
            con_queues + i*3 + 2,
        };
    }
	for (i = 0; i < NR_SERIALS; i++) {
		rs_table[i] = (struct tty_struct) {
			{0, /* no translation */
			0,  /* no translation */
			B2400 | CS8,
			0,
			0,
			INIT_C_CC},
			-1,
			0,
			0,
			0,
			{25, 80, 0, 0},
			rs_write,
			rs_queues+0+i*3,rs_queues+1+i*3,rs_queues+2+i*3
		};
	}
	for (i = 0; i < NR_PTYS; i++) {
		mpty_table[i] = (struct tty_struct) {
			{0, /* no translation */
			0,  /* no translation */
			B9600 | CS8,
			0,
			0,
			INIT_C_CC},
			-1,
			0,
			0,
			0,
			{25, 80, 0, 0},
			mpty_write,
			mpty_queues+0+i*3,mpty_queues+1+i*3,mpty_queues+2+i*3
		};
		spty_table[i] = (struct tty_struct) {
			{0, /* no translation */
			0,  /* no translation */
			B9600 | CS8,
			IXON | ISIG | ICANON,
			0,
			INIT_C_CC},
			-1,
			0,
			0,
			0,
			{25, 80, 0, 0},
			spty_write,
			spty_queues+0+i*3,spty_queues+1+i*3,spty_queues+2+i*3
		};
	}
	rs_init();
	printk("%d virtual consoles\n\r", NR_CONSOLES);
	printk("%d pty's\n\r", NR_PTYS);
	// lp_init();
}