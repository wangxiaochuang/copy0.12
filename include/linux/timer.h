#ifndef _TIMER_H
#define _TIMER_H

#define BLANK_TIMER	0
#define BEEP_TIMER	1

#define SER1_TIMER	2
#define SER2_TIMER	3
#define SER3_TIMER	4
#define SER4_TIMER	5

#define SER1_TIMEOUT	8
#define SER2_TIMEOUT	9
#define SER3_TIMEOUT	10
#define SER4_TIMEOUT	11

#define HD_TIMER	16

struct timer_struct {
    unsigned long expires;
    void (*fn) (void);
};

extern unsigned long timer_active;
extern struct timer_struct timer_table[32];

#endif