#include <linux/msdos_fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/stat.h>
#include <linux/locks.h>

#include <asm/segment.h>

struct super_block *msdos_read_super(struct super_block *s,void *data,
				     int silent) {
    return NULL;
}