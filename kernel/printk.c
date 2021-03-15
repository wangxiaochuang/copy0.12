#include <linux/kernel.h>

#define DEFAULT_MESSAGE_LOGLEVEL 7 /* KERN_DEBUG */
#define DEFAULT_CONSOLE_LOGLEVEL 7 /* anything more serious than KERN_DEBUG */

int console_loglevel = DEFAULT_CONSOLE_LOGLEVEL;

asmlinkage int printk(const char *fmt, ...) {
    return 0;
}