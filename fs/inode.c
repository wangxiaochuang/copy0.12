#include <string.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

extern int *blk_size[];

struct inode inode_table[NR_INODE]={{0, }, };

static inline void wait_on_inode(struct inode *inode) {
	cli();
	while (inode->i_lock) {
		sleep_on(&inode->i_wait);
	}
	sti();
}

static inline void lock_inode(struct inode *inode) {
	cli();
	while (inode->i_lock) {
		sleep_on(&inode->i_wait);
	}
	inode->i_lock = 1;
	sti();
}

static inline void unlock_inode(struct inode *inode) {
	inode->i_lock = 0;
	wake_up(&inode->i_wait);
}

static void write_inode(struct inode * inode)
{
	lock_inode(inode);
	if (!inode->i_dirt || !inode->i_dev) {
		unlock_inode(inode);
		return;
	}
	if (inode->i_op && inode->i_op->write_inode)
		inode->i_op->write_inode(inode);
	unlock_inode(inode);
}

static void read_inode(struct inode *inode) {
	lock_inode(inode);
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->read_inode)
		inode->i_sb->s_op->read_inode(inode);
	unlock_inode(inode);
}

int bmap(struct inode *inode, int block) {
    if (inode->i_op && inode->i_op->bmap)
		return inode->i_op->bmap(inode,block);
	return 0;
}

void invalidate_inodes(int dev) {
    int i;
    struct inode *inode;

    inode = 0 + inode_table;
    for (i = 0; i < NR_INODE; i++, inode++) {
        wait_on_inode(inode);
        if (inode->i_dev == dev) {
            if (inode->i_count) {
                printk("inode in use on removed disk\n\r");
                continue;
            }
            inode->i_dev = inode->i_dirt = 0;
        }
    }
}

void sync_inodes(void) {
    int i;
    struct inode *inode;

    inode = 0 + inode_table;
    for (i = 0; i < NR_INODE; i++, inode++) {
        wait_on_inode(inode);
        // i节点已被修改且不是管道节点
        if (inode->i_dirt && !inode->i_pipe) {
            write_inode(inode);
        }
    }
}

void iput(struct inode * inode) {
    if (!inode) {
        return;
    }
    wait_on_inode(inode);
    if (!inode->i_count) {
        panic("iput: trying to free free inode");
    }
    if (inode->i_pipe) {
        wake_up(&inode->i_wait);
        wake_up(&inode->i_wait2);
        if (--inode->i_count) {
            return;
        }
        // 对于管道节点，inode->i_size存放着内存页地址
        free_page(inode->i_size);
        inode->i_count = 0;
        inode->i_dirt = 0;
        inode->i_pipe = 0;
        return;
    }
    if (!inode->i_dev) {
        inode->i_count--;
        return;
    }
    /* 如果是块设备文件的i节点，则i_zone[0]中是设备号，则刷新该设备。并等待i节点解锁 */
    if (S_ISBLK(inode->i_mode)) {
        sync_dev(inode->i_rdev);
        wait_on_inode(inode);
    }
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;
        return;
    }
    /* 如果i节点的链接数为0，则说明i节点对应文件被删除 */
    if (!inode->i_nlink) {
        if (inode->i_op && inode->i_op->put_inode)
			inode->i_op->put_inode(inode);
        return;
    }
    if (inode->i_dirt) {
        write_inode(inode);
        wait_on_inode(inode);
        goto repeat;
    }
    inode->i_count--;
    return;
}

struct m_inode * get_empty_inode(void) {
	struct inode * inode;
	static struct inode * last_inode = inode_table;	/* 指向i节点表第1项 */
	int i;

    do {
        inode = NULL;
        for (i = NR_INODE; i; i--) {
            // 从头开始搜索
            if (++last_inode >= inode_table + NR_INODE) {
                last_inode = inode_table;
            }
            if (!last_inode->i_count) {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock) {
                    break;
                }
            }
        }
        if (!inode) {
            panic("No free inodes in mem");
        }
        wait_on_inode(inode);
        while (inode->i_dirt) {
            write_inode(inode);
            wait_on_inode(inode);
        }
    } while (inode->i_count);
    memset(inode, 0, sizeof(*inode));
    inode->i_count = 1;
    return inode;
}

struct inode * get_pipe_inode(void) {
    struct inode *inode;

    if (!(inode = get_empty_inode())) {
        return NULL;
    }
    // i_size 指向缓冲区
    if (!(inode->i_size = get_free_page())) {
        inode->i_count = 0;
        return NULL;
    }
    inode->i_count = 2;     // 管道读写指针都会指向这个inode
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;
    inode->i_pipe = 1;      // 将管道使用标志置位
    return inode;
}

/**
 * 读取节点号为nr的i节点内容到内存i节点表中 
 **/
struct inode * iget(int dev, int nr) {
    struct inode *inode, *empty;

    if (!dev) {
        panic("iget with dev==0");
    }
    empty = get_empty_inode();
    inode = inode_table;
    while (inode < NR_INODE + inode_table) {
        if (inode->i_dev != dev || inode->i_ino != nr) {
            inode++;
            continue;
        }
        wait_on_inode(inode);
        if (inode->i_dev != dev || inode->i_ino != nr) {
            inode = inode_table;
            continue;
        }
        inode->i_count++;
        if (inode->i_mount) {
            int i;
            for (i = 0; i < NR_SUPER; i++) {
                if (super_block[i].s_covered == inode) {
                    break;
                }
            }
            if (i >= NR_SUPER) {
                printk("Mounted inode hasn't got sb\n");
                if (empty) {
                    iput(empty);
                }
                return inode;
            }
            iput(inode);
            if (!(inode = super_block[i].s_mounted))
                printk("iget: mounted dev has no rootinode\n");
            else {
                inode->i_count++;
                wait_on_inode(inode);
            }
        }
        if (empty) {
            iput(empty);
        }
        return inode;
    }
    if (!empty) {
        return (NULL);
    }
    inode = empty;
    if (!(inode->i_sb = get_super(dev))) {
        printk("iget: gouldn't get super-block\n\t");
		iput(inode);
		return NULL;
    }
    inode->i_dev = dev;
    inode->i_ino = nr;
    read_inode(inode);
    return inode;
}
