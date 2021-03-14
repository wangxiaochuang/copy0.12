#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

struct bh_struct {
	void (*routine)(void *);
	void *data;
};

extern unsigned long bh_active;
extern unsigned long bh_mask;
extern struct bh_struct bh_base[32];

enum {
	TIMER_BH = 0,
	CONSOLE_BH,
	SERIAL_BH,
	TTY_BH,
	INET_BH,
	KEYBOARD_BH
};

#endif