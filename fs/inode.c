#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/system.h>

static struct inode_hash_entry {
	struct inode * inode;
	int updating;
} hash_table[NR_IHASH];

static struct inode *first_inode;

unsigned long inode_init(unsigned long start, unsigned long end) {
    memset(hash_table, 0, sizeof(hash_table));
    first_inode = NULL;
    return start;
}