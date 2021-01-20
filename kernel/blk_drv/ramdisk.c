#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <asm/system.h>

#define MAJOR_NR 1
#include "blk.h"

char	*rd_start;
int	rd_length = 0;

void do_rd_request(void) {}

long rd_init(long mem_start, int length) {
    int i;
    char *cp;

    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    rd_start = (char *) mem_start;
    rd_length = length;
    cp = rd_start;
    for (i = 0; i < length; i++) {
        *cp++ = '\0';
    }
    return(length);
}

void rd_load(void) {
    struct buffer_head *bh;
    struct super_block s;
    int block = 256;
    int i = 1;
    int nblocks;
    char *cp;

    if (!rd_length)
        return;
    panic("ROOT_DEV: %d, %d", MAJOR(ROOT_DEV), ROOT_DEV);
    if (MAJOR(ROOT_DEV) != 2)
        return;

}