#ifndef _LINUX_TTY_H
#define _LINUX_TTY_H

#include <linux/termios.h>

struct screen_info {
	unsigned char  orig_x;
	unsigned char  orig_y;
	unsigned char  unused1[2];
	unsigned short orig_video_page;
	unsigned char  orig_video_mode;
	unsigned char  orig_video_cols;
	unsigned short orig_video_ega_ax;
	unsigned short orig_video_ega_bx;
	unsigned short orig_video_ega_cx;
	unsigned char  orig_video_lines;
};

#define TTY_BUF_SIZE 1024

struct tty_queue {
	unsigned long head;
	unsigned long tail;
	struct wait_queue * proc_list;
	unsigned char buf[TTY_BUF_SIZE];
};

struct tty_struct {
	struct termios *termios;
	int pgrp;
	int session;
	unsigned char stopped:1, hw_stopped:1, packet:1, lnext:1;
	unsigned char char_error:3;
	unsigned char erasing:1;
	unsigned char ctrl_status;
	short line;
	int disc;
	int flags;
	int count;
	unsigned int column;
	struct winsize winsize;
	int  (*open)(struct tty_struct * tty, struct file * filp);
	void (*close)(struct tty_struct * tty, struct file * filp);
	void (*write)(struct tty_struct * tty);
	int  (*ioctl)(struct tty_struct *tty, struct file * file,
		    unsigned int cmd, unsigned long arg);
	void (*throttle)(struct tty_struct * tty, int status);
	void (*set_termios)(struct tty_struct *tty, struct termios * old);
	void (*stop)(struct tty_struct *tty);
	void (*start)(struct tty_struct *tty);
	void (*hangup)(struct tty_struct *tty);
	struct tty_struct *link;
	unsigned char *write_data_ptr;
	int write_data_cnt;
	void (*write_data_callback)(void * data);
	void * write_data_arg;
	int readq_flags[TTY_BUF_SIZE/32];
	int secondary_flags[TTY_BUF_SIZE/32];
	int canon_data;
	unsigned long canon_head;
	unsigned int canon_column;
	struct tty_queue read_q;
	struct tty_queue write_q;
	struct tty_queue secondary;
	void *disc_data;
};

extern long tty_init(long);

#endif