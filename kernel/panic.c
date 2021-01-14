#include <stdarg.h>
#include <stddef.h>
#include <linux/kernel.h>

// volatile void panic(const char *s) {
// @todo
static char buf[64] = {'K', 'e', 'r', 'n', 'e', 'l', ' ', 'p', 'a', 'n', 'i', 'c', ':', ' '};
extern int vsprintf(char * buf, const char * fmt, va_list args);

void panic(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf + 14, fmt, args);
    va_end(args);
    console_print(buf);

    for (;;){
        __asm__ ("hlt"::);
    };
}