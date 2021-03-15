#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/segment.h>
#include <asm/system.h>

long tty_init(long kmem_start) {
    int i;
    if (sizeof(struct tty_struct) > PAGE_SIZE)
		panic("size of tty structure > PAGE_SIZE!");
    return 0;
}