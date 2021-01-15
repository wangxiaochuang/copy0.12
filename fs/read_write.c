#include <linux/kernel.h>
int sys_write(unsigned int fd, char * buf, int count) {
    console_print(buf);
    return count;
}