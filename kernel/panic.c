#include <stdarg.h>
#include <linux/sched.h>
#include <linux/kernel.h>

extern int vsprintf(char * buf, const char * fmt, va_list args);
extern void console_print(const char *);

NORET_TYPE void panic(const char * fmt, ...) {
    static char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    printk(KERN_EMERG "Kernel panic: %s\n", buf);
    for(;;);
}

static void raw_print(const char * b) {
	unsigned char c;
	unsigned char *video = (unsigned char *) 0xb8000;
	while ((c = *(b++)) != 0) {
		video[0] = c;
		video[1] = 0x07;
		video += 2;
	}
}

NORET_TYPE void mypanic(const char * fmt, ...) {
    static char mybuf[128] = {'[', '['};
    va_list args;
    int len = 0;

    va_start(args, fmt);
    len = vsprintf(mybuf+2, fmt, args);
    va_end(args);
    mybuf[len + 2] = ']';
    mybuf[len + 3] = ']';
    mybuf[len + 4] = '\0';
    raw_print(mybuf);
    for(;;);
}