#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

struct timer_struct {
	unsigned long expires;
	void (*fn)(void);
};

struct timer_list {
	struct timer_list *next;
	struct timer_list *prev;
	unsigned long expires;
	unsigned long data;
	void (*function) (unsigned long);
};

#endif