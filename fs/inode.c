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

void sync_inodes(void) {
    int i;
    struct m_inode *inode;
    inode = 0 + inode_table;
    for (i = 0; i < NR_INODE; i++, inode++) {
        wait_on_inode(inode);
        // i节点已被修改且不是管道节点
        if (inode->i_dirt && !inode->i_pipe) {
            write_inode(inode);
        }
    }
}

// 目的是通过相对的块偏移找到真实数据所在绝对逻辑块号
static int _bmap(struct m_inode * inode, int block, int create) {
    struct buffer_head *bh;
    int i;

    if (block < 0) {
        panic("_bmap: block < 0");
    }
    /* block >= 直接块数 + 间接块数 + 二次间接块数 */
    if (block >= 7 + 512 + 512 * 512) {
        panic("_bmap: block > big");
    }
    if (block < 7) {
        if (create && !inode->i_zone[block]) {
            /*
            if ((inode->i_zone[block] = new_block(inode->i_dev))) {
                inode->i_ctime = CURRENT_TIME;
                inode->i_dirt = 1;
            }
            */
        }
        return inode->i_zone[block];
    }
    block -= 7;
    if (block < 512) {
        if (create && !inode->i_zone[7]) {
            // @todo
        }
        if (!inode->i_zone[7]) {
            return 0;
        }
        if (!(bh = bread(inode->i_dev, inode->i_zone[7]))) {
            return 0;
        }
        i = ((unsigned short *) (bh->b_data))[block];
        if (create && !i) {
            // @todo
        }
        brelse(bh);
        return 1;
    }

    block -= 512;
    if (create && !inode->i_zone[8]) {
        // @todo
    }
    if (!inode->i_zone[8]) {
        return 0;
    }
    if (!(bh = bread(inode->i_dev, inode->i_zone[8]))) {
        return 0;
    }
    i = ((unsigned short *) bh->b_data)[block >> 9];
    if (create && !i) {
        // @todo
    }
    brelse(bh);
    if (!i) {
        return 0;
    }
    /* 读取二次间接块的二级块 */
    if (!(bh = bread(inode->i_dev, i))) {
        return 0;
    }
    i = ((unsigned short *)bh->b_data)[block & 511];
    if (create && !i) {
        // @todo
    }
    brelse(bh);
    return i;
}

int bmap(struct m_inode *inode, int block) {
    return _bmap(inode, block, 0);
}

void iput(struct m_inode * inode) {
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
        sync_dev(inode->i_zone[0]);
        wait_on_inode(inode);
    }
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;
        return;
    }
    /* 如果i节点的链接数为0，则说明i节点对应文件被删除 */
    if (!inode->i_nlinks) {
        truncate(inode);
        free_inode(inode);
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
    struct super_block *sb;
    struct buffer_head *bh;
    int block;

    lock_inode(inode);
    if (!inode->i_dirt || !inode->i_dev) {
        unlock_inode(inode);
        return;
    }
    // 不可能没有注册超级块
    if (!(sb = get_super(inode->i_dev))) {
        panic("trying to write inode without device");
    }
    /* 该i节点所在逻辑块号 = 
    （启动块 + 超级块）+ i节点位图块数 + 逻辑块位图块数 + （i节点号 - 1）/每块含有的i节点数 */
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks
        + (inode->i_num - 1) / INODES_PER_BLOCK;
    if (!(bh = bread(inode->i_dev, block))) {
        panic("unable to read i-node block");
    }
    // 注意这里是模除，取的是该inode在这一个块中的偏移
    ((struct d_inode *)bh->b_data)[(inode->i_num - 1) % INODES_PER_BLOCK]
        = *(struct d_inode *)inode;
    // 只是修改了bh缓冲区，所以置位，inode已经与缓冲区一致所以清位
    bh->b_dirt = 1;
    inode->i_dirt = 0;

    brelse(bh);
    unlock_inode(inode);
}