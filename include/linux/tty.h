#ifndef _LINUX_TTY_H
#define _LINUX_TTY_H

#include <linux/termios.h>

#define NR_CONSOLES	8
#define NR_LDISCS	16

// setup.S里将这些信息写到0x90000处，main.c里设置了screen_info结构体变量
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

extern struct screen_info screen_info;

#define ORIG_X			(screen_info.orig_x)
#define ORIG_Y			(screen_info.orig_y)
#define ORIG_VIDEO_PAGE		(screen_info.orig_video_page)
#define ORIG_VIDEO_MODE		(screen_info.orig_video_mode)
#define ORIG_VIDEO_COLS 	(screen_info.orig_video_cols)
#define ORIG_VIDEO_EGA_AX	(screen_info.orig_video_ega_ax)
#define ORIG_VIDEO_EGA_BX	(screen_info.orig_video_ega_bx)
#define ORIG_VIDEO_EGA_CX	(screen_info.orig_video_ega_cx)
#define ORIG_VIDEO_LINES	(screen_info.orig_video_lines)

#define VIDEO_TYPE_MDA		0x10	/* Monochrome Text Display	*/
#define VIDEO_TYPE_CGA		0x11	/* CGA Display 			*/
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA in Monochrome Mode	*/
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA in Color Mode	*/

#define __DISABLED_CHAR '\0'

#define TTY_BUF_SIZE 1024

struct tty_queue {
	unsigned long head;
	unsigned long tail;
	struct wait_queue * proc_list;
	unsigned char buf[TTY_BUF_SIZE];
};

#define IS_A_CONSOLE(min)	(((min) & 0xC0) == 0x00)
#define IS_A_SERIAL(min)	(((min) & 0xC0) == 0x40)
#define IS_A_PTY(min)		((min) & 0x80)
#define IS_A_PTY_MASTER(min)	(((min) & 0xC0) == 0x80)
#define IS_A_PTY_SLAVE(min)	(((min) & 0xC0) == 0xC0)
#define PTY_OTHER(min)		((min) ^ 0x40)

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

struct tty_ldisc {
	int	flags;
	/*
	 * The following routines are called from above.
	 */
	int	(*open)(struct tty_struct *);
	void	(*close)(struct tty_struct *);
	int	(*read)(struct tty_struct * tty, struct file * file,
			unsigned char * buf, unsigned int nr);
	int	(*write)(struct tty_struct * tty, struct file * file,
			 unsigned char * buf, unsigned int nr);	
	int	(*ioctl)(struct tty_struct * tty, struct file * file,
			 unsigned int cmd, unsigned long arg);
	int	(*select)(struct tty_struct * tty, struct inode * inode,
			  struct file * file, int sel_type,
			  struct select_table_struct *wait);
	/*
	 * The following routines are called from below.
	 */
	void	(*handler)(struct tty_struct *);
};

#define LDISC_FLAG_DEFINED	0x00000001

#define TTY_WRITE_BUSY 0
#define TTY_READ_BUSY 1
#define TTY_SQ_THROTTLED 2
#define TTY_RQ_THROTTLED 3
#define TTY_IO_ERROR 4
#define TTY_SLAVE_CLOSED 5
#define TTY_EXCLUSIVE 6

#define TTY_BREAK	1
#define TTY_FRAME	2
#define TTY_PARITY	3
#define TTY_OVERRUN	4

#define TTY_WRITE_FLUSH(tty) tty_write_flush((tty))
#define TTY_READ_FLUSH(tty) tty_read_flush((tty))

extern int fg_console;
extern unsigned long video_num_columns;
extern unsigned long video_num_lines;
extern struct wait_queue * keypress_wait;

#define TTY_TABLE_IDX(nr)	((nr) ? (nr) : (fg_console+1))
#define TTY_TABLE(nr) 		(tty_table[TTY_TABLE_IDX(nr)])

extern long rs_init(long);
extern long con_init(long);
extern long tty_init(long);

/* console.c */

extern int con_open(struct tty_struct * tty, struct file * filp);
extern void update_screen(int new_console);
extern void blank_screen(void);
extern void unblank_screen(void);

#endif