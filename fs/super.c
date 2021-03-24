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

struct file_system_type *get_fs_type(char *name) {
    int a;

    if (!name)
        return &file_systems[0];
    for (a = 0; file_systems[a].read_super; a++)
        if (!strcmp(name, file_systems[a].name))
            return (&file_systems[a]);
    return NULL;
}

void __wait_on_super(struct super_block * sb) {
    struct wait_queue wait = { current, NULL };

    add_wait_queue(&sb->s_wait, &wait);
repeat:
    current->state = TASK_UNINTERRUPTIBLE;
    if (sb->s_lock) {
        schedule();
        goto repeat;
    }
    remove_wait_queue(&sb->s_wait, &wait);
    current->state = TASK_RUNNING;
}

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

static struct super_block * get_super(dev_t dev) {
    struct super_block *s;

    if (!dev)
        return NULL;
    s = 0 + super_blocks;
    while (s < NR_SUPER + super_blocks)
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev)
                return s;
            s = 0 + super_blocks;
        } else
            s++;
    return NULL;
}

static struct super_block * read_super(dev_t dev,char *name,int flags,
				       void *data, int silent) {
    struct super_block *s;
    struct file_system_type *type;

    if (!dev)
        return NULL;
    // check_disk_change(dev);
    s = get_super(dev);
    if (s)
        return s;
    if (!(type = get_fs_type(name))) {
        printk("VFS: on device %d/%d: get_fs_type(%s) failed\n",
						MAJOR(dev), MINOR(dev), name);
		return NULL;
    }
    for (s = 0 + super_blocks; ;s++) {
        if (s >= NR_SUPER + super_blocks)
            return NULL;
        if (!s->s_dev)
            break;
    }
    s->s_dev = dev;
    s->s_flags = flags;
    if (!type->read_super(s, data, silent)) {
        s->s_dev = 0;
        return NULL;
    }
    s->s_dev = dev;
    s->s_covered = NULL;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    return s;
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