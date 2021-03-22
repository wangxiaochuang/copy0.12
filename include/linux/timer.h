#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#define BLANK_TIMER	0

#define HD_TIMER	16
#define FLOPPY_TIMER	17
#define COPRO_TIMER	21

struct timer_struct {
	unsigned long expires;
	void (*fn)(void);
};

extern unsigned long timer_active;
extern struct timer_struct timer_table[32];

struct timer_list {
	struct timer_list *next;
	struct timer_list *prev;
	unsigned long expires;
	unsigned long data;
	void (*function) (unsigned long);
};

#endif