#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

struct super_block super_block[NR_SUPER];
int ROOT_DEV = 0;

struct super_block * get_super(int dev) {
    return NULL;
}

static struct super_block * read_super(int dev) {
	struct super_block * s;
	struct buffer_head * bh;
	int i, block;

	if (!dev) {
		return NULL;
	}
    /* 检查软盘是否更换 */
	// check_disk_change(dev);
    if ((s = get_super(dev))) {
        return s;
    }
}

void mount_root(void) {
    int i, free;
    struct super_block *p;
    struct m_inode *mi;

    if (32 != sizeof (struct d_inode)) {
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
    if (!(p = read_super(ROOT_DEV))) {
        panic("Unable to mount root");
    }
}