#include <string.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

extern int *blk_size[];

struct m_inode inode_table[NR_INODE]={{0, }, };

static void read_inode(struct m_inode *inode);		/* 读指定i节点号的i节点信息 */
static void write_inode(struct m_inode *inode);

static inline void wait_on_inode(struct m_inode *inode) {
	cli();
	while (inode->i_lock) {
		sleep_on(&inode->i_wait);
	}
	sti();
}

static inline void lock_inode(struct m_inode *inode) {
	cli();
	while (inode->i_lock) {
		sleep_on(&inode->i_wait);
	}
	inode->i_lock = 1;
	sti();
}

static inline void unlock_inode(struct m_inode *inode) {
	inode->i_lock = 0;
	wake_up(&inode->i_wait);
}

void iput(struct m_inode * inode) {
    
}

struct m_inode * get_empty_inode(void) {
	struct m_inode * inode;
	static struct m_inode * last_inode = inode_table;	/* 指向i节点表第1项 */
	int i;

    do {
        inode = NULL;
        for (i = NR_INODE; i; i--) {
            if (++last_inode >= inode_table + NR_INODE) {
                last_inode = inode_table;
            }
            if (!last_inode->i_count) {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock) {
                    break;
                }
                // @todo
                inode = NULL;
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

/**
 * 读取节点号为nr的i节点内容到内存i节点表中 
 **/
struct m_inode * iget(int dev, int nr) {
    struct m_inode *inode, *empty;

    if (!dev) {
        panic("iget with dev==0");
    }
    empty = get_empty_inode();
    inode = inode_table;
    while (inode < NR_INODE + inode_table) {
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode++;
            continue;
        }
        wait_on_inode(inode);
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode = inode_table;
            continue;
        }
        inode->i_count++;
        if (inode->i_mount) {
            int i;
            for (i = 0; i < NR_SUPER; i++) {
                if (super_block[i].s_imount == inode) {
                    break;
                }
            }
            if (i >= NR_SUPER) {
                if (empty) {
                    iput(empty);
                }
                return inode;
            }
            iput(inode);
            dev = super_block[i].s_dev;
            nr = ROOT_INO;
            inode = inode_table;
            continue;
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
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);
    return inode;
}

static void read_inode(struct m_inode *inode) {
    struct super_block *sb;
    struct buffer_head *bh;
    int block;
    
    lock_inode(inode);
    if (!(sb = get_super(inode->i_dev))) {
        panic("trying to read inode without dev");
    }
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
        (inode->i_num - 1) / INODES_PER_BLOCK;
    if (!(bh = bread(inode->i_dev, block))) {
        panic("unable to read i-node block");
    }
    *(struct d_inode *)inode = 
		((struct d_inode *)bh->b_data)[(inode->i_num - 1) % INODES_PER_BLOCK];
	/* 释放缓冲块，并解锁该i节点 */
	brelse(bh);

    if (S_ISBLK(inode->i_mode)) {
        int i = inode->i_zone[0];
        if (blk_size[MAJOR(i)]) {
            inode->i_size = 1024 * blk_size[MAJOR(i)][MINOR(i)];
        } else {
            inode->i_size = 0x7fffffff;
        }
    }
    unlock_inode(inode);
}

static void write_inode(struct m_inode * inode) {

}