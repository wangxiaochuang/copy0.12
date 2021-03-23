#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/locks.h>

#include <asm/system.h>
#include <asm/segment.h>

extern struct file_system_type file_systems[];

extern int root_mountflags;

struct super_block super_blocks[NR_SUPER];

dev_t ROOT_DEV = 0;

static struct file_lock file_lock_table[NR_FILE_LOCKS];
static struct file_lock *file_lock_free_list;

void fcntl_init_locks(void) {
    struct file_lock *fl;

    for (fl = &file_lock_table[0]; fl < file_lock_table + NR_FILE_LOCKS - 1; fl++) {
        fl->fl_next = fl + 1;
        fl->fl_owner = NULL;
    }
    file_lock_table[NR_FILE_LOCKS - 1].fl_next = NULL;
    file_lock_table[NR_FILE_LOCKS - 1].fl_owner = NULL;
    file_lock_free_list = &file_lock_table[0];
}

static struct super_block * read_super(dev_t dev,char *name,int flags,
				       void *data, int silent) {
    struct super_block *s;
    struct file_system_type *type;

    if (!dev)
        return NULL;
    return NULL;
}

void mount_root(void) {
    struct file_system_type *fs_type;
    struct super_block *sb;
    struct inode *inode;

    memset(super_blocks, 0, sizeof(super_blocks));
    fcntl_init_locks();
    /*
    if (MAJOR(ROOT_DEV) == FLOPPY_MAJOR) {
        printk(KERN_NOTICE "VFS: Insert root floppy and press ENTER\n");
		wait_for_keypress();
    }
    */
    for (fs_type = file_systems; fs_type->read_super; fs_type++) {
        if (!fs_type->requires_dev)
            continue;
        sb = read_super(ROOT_DEV, fs_type->name, root_mountflags, NULL, 1);
    }
}