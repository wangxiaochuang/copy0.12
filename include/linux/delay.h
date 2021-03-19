#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

extern unsigned long loops_per_sec;

static __inline__ void __delay(int loops) {
    __asm__(".align 2, 0x90\n1:\tdecl %0\n\tjns 1b"::"a" (loops));
}

static __inline__ void udelay(unsigned long usecs) {
    usecs *= 0x000010c6;        /* 2^32 / 1000000 */
    __asm__("mull %0"
    :"=d" (usecs)
    :"a" (usecs), "0" (loops_per_sec));
    __delay(usecs);
}

#endif