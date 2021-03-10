#include <linux/config.h>
#include <linux/sched.h>
#include <linux/minix_fs.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

/**
 * 当addr的bitnr位是1，则返回1，否则返回0
 **/
#define set_bit(bitnr, addr) ({ 											\
	register int __res __asm__("ax");										\
	__asm__("bt %2, %3; setb %%al"											\
			:"=a" (__res)													\
			:"a" (0), "r" (bitnr),"m" (*(addr))); 							\
	__res; })

struct super_block super_block[NR_SUPER];
int ROOT_DEV = 0;

static struct file_system_type file_systems[] = {
    { minix_read_super, "minix"},
    { NULL, NULL }
};

struct file_system_type *get_fs_type(char *name) {
    int a;
    for (a = 0; file_systems[a].read_super; a++)
        if (!strcmp(name, file_systems[a].name))
            return &file_systems[a];
    return NULL;
}

void lock_super(struct super_block *sb) {
    cli();
    while (sb->s_lock) {
        sleep_on(&(sb->s_wait));
    }
    sb->s_lock = 1;
    sti();
}

void free_super(struct super_block *sb) {
    cli();
    sb->s_lock = 0;
    wake_up(&(sb->s_wait));
    sti();
}

static void wait_on_super(struct super_block * sb) {
	cli();
	while (sb->s_lock) {
		sleep_on(&(sb->s_wait));
	}
	sti();
}

struct super_block * get_super(int dev) {
    struct super_block *s;
    if (!dev) {
        return NULL;
    }
    s = 0 + super_block;
    while (s < NR_SUPER + super_block) {
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev) {
                return s;
            }
            s = 0 + super_block;
        } else {
            s++;
        }
    }
    return NULL;
}

void put_super(int dev) {
    struct super_block *sb;

    if (dev == ROOT_DEV) {
        printk("root diskette changed: prepare for armageddon\n\r");
		return;
    }
    if (!(sb = get_super(dev))) {
        return;
    }
    // 正挂载着不能释放
    if (sb->s_covered) {
        printk("Mounted disk changed - tssk, tssk\n\r");
		return;
    }
    if (sb->s_op && sb->s_op->put_super)
        sb->s_op->put_super(sb);
}

static struct super_block * read_super(int dev, char *name, void *data) {
	struct super_block * s;
    struct file_system_type *type;

	if (!dev) {
		return NULL;
	}
    /* 检查软盘是否更换 */
	// check_disk_change(dev);

    if ((s = get_super(dev))) {
        return s;
    }
    if (!(type = get_fs_type(name))) {
		printk("get fs type failed %s\n",name);
		return NULL;
    }
    for (s = 0 + super_block; ; s++) {
        if (s >= NR_SUPER + super_block) {
            return NULL;
        }
        if (!s->s_dev) {
            break;
        }
    }
    s->s_dev = dev;
    if (!type->read_super(s, data))
        return NULL;
    s->s_dev = dev;
    s->s_covered = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    return s;
}

int sys_umount(char *dev_name) {
	struct inode *inode;
	struct super_block *sb;
	int dev;

    if (!(inode = namei(dev_name))) return -ENOENT;
    dev = inode->i_rdev;
    if (!S_ISBLK(inode->i_mode)) {
        iput(inode);
        return -ENOTBLK;
    }
    iput(inode);
    // 根文件系统不能被卸载
    if (dev == ROOT_DEV) return -EBUSY;
    // s_covered没有说明压根没有挂载
    if (!(sb = get_super(dev)) || !(sb->s_covered)) return -ENOENT;
    if (!sb->s_covered->i_mount) {
        printk("Mounted inode has i_mount=0\n");
    }
    // 既然要卸载这个设备，那么就不能有相关的inode关联它
    for (inode = inode_table + 0; inode < inode_table + NR_INODE; inode++) {
        // 遍历inode列表，发现有inode正在使用这个设备且引用计数不为0
        if (inode->i_dev == dev && inode->i_count) {
            // 只会剩最后一个挂载点
            if (inode == sb->s_mounted && inode->i_count == 1)
                continue;
            else
                return -EBUSY;
        }
    }
    sb->s_covered->i_mount = 0;
    iput(sb->s_covered);
    sb->s_covered = NULL;
    // 释放文件系统的根i节点
    iput(sb->s_mounted);
    sb->s_mounted = NULL;
    put_super(dev);
    sync_dev(dev);
    return 0;
}

/**
 * 将设备dev挂载到dir上
 **/
int sys_mount(char *dev_name, char *dir_name, int rw_flag) {
	struct inode *dev_i, *dir_i;
	struct super_block * sb;
	int dev;

    if (!(dev_i = namei(dev_name))) return -ENOENT;
    dev = dev_i->i_rdev;
    if (!S_ISBLK(dev_i->i_mode)) {
        iput(dev_i);
        return -EPERM;
    }
    iput(dev_i);
    if (!(dir_i = namei(dir_name))) return -ENOENT;
    // 该i节点的引用数不是1（只能在这里引用），或者是根文件系统
    if (dir_i->i_count != 1 || dir_i->i_ino == MINIX_ROOT_INO) {
        iput(dir_i);
        return -EBUSY;
    }
    if (!S_ISDIR(dir_i->i_mode)) {
        iput(dir_i);
        return -EPERM;
    }
    // 已经有设备挂载到dir_name了
    if (dir_i->i_mount) {
        iput(dir_i);
        return -EPERM;
    }
    if (!(sb = read_super(dev, "minix", NULL))) {
        iput(dir_i);
        return -EBUSY;
    }
    // 设备已经挂载在其他地方了
    if (sb->s_covered) {
        iput(dir_i);
        return -EBUSY;
    }
    sb->s_covered = dir_i;
    dir_i->i_mount = 1;
    dir_i->i_dirt = 1;
    return 0;
}

void mount_root(void) {
    int i, free;
    struct super_block *p;
    struct inode *mi;

    if (32 != sizeof (struct minix_inode)) {
		panic("bad i-node size");
	}

    for (i = 0; i < NR_FILE; i++) {
        file_table[i].f_count = 0;
    }
    if (MAJOR(ROOT_DEV) == 2) {
        panic("need root floppy");
    }
    for (p = &super_block[0]; p < &super_block[NR_SUPER]; p++) {
		p->s_dev = 0;
		p->s_lock = 0;
		p->s_wait = NULL;
    }
    if (!(p = read_super(ROOT_DEV, "minix", NULL))) {
        panic("Unable to mount root");
    }
    mi = p->s_mounted;
    mi->i_count += 3;
    p->s_mounted = p->s_covered = mi;
    current->pwd = mi;
    current->root = mi;
    free = 0;
    i = p->s_nzones;
    while (--i >= 0) {
        if (!set_bit(i & 8191, p->s_zmap[i>>13]->b_data)) {
			free++;
		}
    }
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    free = 0;
    i = p->s_ninodes + 1;
    while (--i >= 0) {
        if (!set_bit(i & 8191, p->s_imap[i>>13]->b_data)) {
            free++;
        }
    }
    printk("%d/%d free inodes\n\r", free, p->s_ninodes);
}