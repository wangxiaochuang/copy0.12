#include <stdarg.h>

#include <asm/segment.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>

struct super_block * ext2_read_super (struct super_block * sb, void * data,
				      int silent) {
    return NULL;
}