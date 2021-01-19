#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>

static char buf[256];

extern int vsprintf(char * buf, const char * fmt, va_list args);

int printk(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    console_print(buf);
    return i;
}

int dumpmem(const void *address, int len) {
    const unsigned char *p = address;
    int i;
    for (i = 0; i < len; i++) {
        if (i == 0)
            printk("0x");
        printk("%02X", p[i] & 0xff);
    }
    printk("\n\r");
}