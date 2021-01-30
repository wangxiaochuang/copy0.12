#include <errno.h>
#include <termios.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>

extern int session_of_pgrp(int pgrp);
extern int tty_signal(int sig, struct tty_struct *tty);

static unsigned short quotient[] = {
	0, 2304, 1536, 1047, 857,
	768, 576, 384, 192, 96,
	64, 48, 24, 12, 6, 3
};

static void change_speed(struct tty_struct * tty) {
	unsigned short port,quot;

	if (!(port = tty->read_q->data))
		return;
	quot = quotient[tty->termios.c_cflag & CBAUD];
	cli();
	outb_p(0x80,port+3);		/* set DLAB */
	outb_p(quot & 0xff,port);	/* LS of divisor */
	outb_p(quot >> 8,port+1);	/* MS of divisor */
	outb(0x03,port+3);		/* reset DLAB */
	sti();
}

static void flush(struct tty_queue * queue) {
	cli();
	queue->head = queue->tail;
	sti();
}

static void wait_until_sent(struct tty_struct * tty) {
	/* do nothing - not implemented */
}

static void send_break(struct tty_struct * tty) {
	/* do nothing - not implemented */
}

static int get_termios(struct tty_struct * tty, struct termios * termios) {
    int i;
    verify_area(termios, sizeof (*termios));
    for (i = 0; i < (sizeof (*termios)); i++)
        put_fs_byte(((char *) &tty->termios)[i], i + (char *) termios);
    return 0;
}

static int set_termios(struct tty_struct * tty, struct termios * termios,
			int channel) {
    int i, retsig;
    if ((current->tty == channel) && (tty->pgrp != current->pgrp)) {
        retsig = tty_signal(SIGTTOU, tty);
        if (retsig == -ERESTARTSYS || retsig == -EINTR)
            return retsig;
    }
    for (i=0 ; i< (sizeof (*termios)) ; i++)
		((char *)&tty->termios)[i]=get_fs_byte(i+(char *)termios);
	change_speed(tty);
	return 0;
}

static int get_termio(struct tty_struct * tty, struct termio * termio) {
    int i;
    struct termio tmp_termio;

    verify_area(termio, sizeof(*termio));
    tmp_termio.c_iflag = tty->termios.c_iflag;
	tmp_termio.c_oflag = tty->termios.c_oflag;
	tmp_termio.c_cflag = tty->termios.c_cflag;
	tmp_termio.c_lflag = tty->termios.c_lflag;
	tmp_termio.c_line = tty->termios.c_line;
	for(i=0 ; i < NCC ; i++)
		tmp_termio.c_cc[i] = tty->termios.c_cc[i];
	for (i=0 ; i< (sizeof (*termio)) ; i++)
		put_fs_byte( ((char *)&tmp_termio)[i] , i+(char *)termio );
	return 0;
}

static int set_termio(struct tty_struct * tty, struct termio * termio,
			int channel) {
	int i, retsig;
	struct termio tmp_termio;

	if ((current->tty == channel) && (tty->pgrp != current->pgrp)) {
		retsig = tty_signal(SIGTTOU, tty);
		if (retsig == -ERESTARTSYS || retsig == -EINTR)
			return retsig;
	}
	for (i=0 ; i< (sizeof (*termio)) ; i++)
		((char *)&tmp_termio)[i]=get_fs_byte(i+(char *)termio);
	*(unsigned short *)&tty->termios.c_iflag = tmp_termio.c_iflag;
	*(unsigned short *)&tty->termios.c_oflag = tmp_termio.c_oflag;
	*(unsigned short *)&tty->termios.c_cflag = tmp_termio.c_cflag;
	*(unsigned short *)&tty->termios.c_lflag = tmp_termio.c_lflag;
	tty->termios.c_line = tmp_termio.c_line;
	for(i=0 ; i < NCC ; i++)
		tty->termios.c_cc[i] = tmp_termio.c_cc[i];
	change_speed(tty);
	return 0;
}

int tty_ioctl(int dev, int cmd, int arg) {
    struct tty_struct *tty;
    int pgrp;

    if (MAJOR(dev) == 5) {
        dev = current->tty;
        if (dev < 0)
            panic("tty_ioctl: dev < 0");
    } else
        dev = MINOR(dev);
    tty = tty_table + (dev ? ((dev < 64) ? dev - 1 : dev) : fg_console);
    switch (cmd) {
		// 取终端termios结构信息
		case TCGETS:
			return get_termios(tty,(struct termios *) arg);
		// 设置termios结构信息之前，需先等待输出队列中所有数据处理完毕，并且清空输入队列
		case TCSETSF:
			flush(tty->read_q); /* fallthrough */
		// 设置termios结构信息之前，需先等待输出队列中所有数据处理完毕
		case TCSETSW:
			wait_until_sent(tty); /* fallthrough */
		// 设置termios结构
		case TCSETS:
			return set_termios(tty,(struct termios *) arg, dev);
		// 取终端termios结构信息
		case TCGETA:
			return get_termio(tty,(struct termio *) arg);
		case TCSETAF:
			flush(tty->read_q); /* fallthrough */
		case TCSETAW:
			wait_until_sent(tty); /* fallthrough */
		case TCSETA:
			return set_termio(tty,(struct termio *) arg, dev);
		// 如果参数arg是0，则等待输出队列处理完毕，并发送一个break
		case TCSBRK:
			if (!arg) {
				wait_until_sent(tty);
				send_break(tty);
			}
			return 0;
		// 开始/停止流控制
		case TCXONC:
			switch (arg) {
            // terminal control output 挂起输出
			case TCOOFF:
				tty->stopped = 1;
				tty->write(tty);
				return 0;
            // terminal control output 恢复挂起的输出
			case TCOON:
				tty->stopped = 0;
				tty->write(tty);
				return 0;
            // 要求终端停止输入
			case TCIOFF:
				if (STOP_CHAR(tty))
					PUTCH(STOP_CHAR(tty),tty->write_q);
				return 0;
            // 要求终端恢复
			case TCION:
				if (START_CHAR(tty))
					PUTCH(START_CHAR(tty),tty->write_q);
				return 0;
			}
			return -EINVAL; /* not implemented */
        // 刷新已写输出但还没发送、或已收但还没有读的数据
		case TCFLSH:
			if (arg==0)
				flush(tty->read_q);
			else if (arg==1)
				flush(tty->write_q);
			else if (arg==2) {
				flush(tty->read_q);
				flush(tty->write_q);
			} else
				return -EINVAL;
			return 0;
        // 设置终端串行线路专用模式。
		case TIOCEXCL:
			return -EINVAL; /* not implemented */
        // 复位终端串行线路专用模式
		case TIOCNXCL:
			return -EINVAL; /* not implemented */
		// 设置 tty 为控制终端
		case TIOCSCTTY:
			return -EINVAL; /* set controlling term NI */
		// 读取终端进程组号
		case TIOCGPGRP:
			verify_area((void *) arg,4);
			put_fs_long(tty->pgrp,(unsigned long *) arg);
			return 0;
		// 设置终端进程组号 pgrp
		case TIOCSPGRP:
			if ((current->tty < 0) ||
			    (current->tty != dev) ||
			    (tty->session != current->session))
				return -ENOTTY;
			pgrp=get_fs_long((unsigned long *) arg);
			if (pgrp < 0)
				return -EINVAL;
			if (session_of_pgrp(pgrp) != current->session)
				return -EPERM;
			tty->pgrp = pgrp;			
			return 0;
		// 返回输出队列中还未送出的字符数
		case TIOCOUTQ:
			verify_area((void *) arg,4);
			put_fs_long(CHARS(tty->write_q),(unsigned long *) arg);
			return 0;
		// 返回输入辅助队列中还未读取的字符数
		case TIOCINQ:
			verify_area((void *) arg,4);
			put_fs_long(CHARS(tty->secondary),
				(unsigned long *) arg);
			return 0;
		case TIOCSTI:
			return -EINVAL; /* not implemented */
		case TIOCGWINSZ:
			return -EINVAL; /* not implemented */
		case TIOCSWINSZ:
			return -EINVAL; /* not implemented */
		case TIOCMGET:
			return -EINVAL; /* not implemented */
		case TIOCMBIS:
			return -EINVAL; /* not implemented */
		case TIOCMBIC:
			return -EINVAL; /* not implemented */
		case TIOCMSET:
			return -EINVAL; /* not implemented */
		case TIOCGSOFTCAR:
			return -EINVAL; /* not implemented */
		case TIOCSSOFTCAR:
			return -EINVAL; /* not implemented */
		default:
			return -EINVAL;
    }
}