#include <asm/system.h>
#include <asm/segment.h>

#include <linux/sched.h>
#include <linux/nfs_fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/locks.h>

struct super_block *nfs_read_super(struct super_block *sb, void *raw_data,
				   int silent) {
    return NULL;
}