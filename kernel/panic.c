#include <stdarg.h>
#include <linux/sched.h>
#include <linux/kernel.h>

extern int vsprintf(char * buf, const char * fmt, va_list args);

NORET_TYPE void panic(const char * fmt, ...) {
    static char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    printk(KERN_EMERG "Kernel panic: %s\n", buf);
    for(;;);
}