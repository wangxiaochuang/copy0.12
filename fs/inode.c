#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <asm/system.h>

static struct inode_hash_entry {
	struct inode * inode;
	int updating;
} hash_table[NR_IHASH];

static struct inode * first_inode;
static struct wait_queue * inode_wait = NULL;
static int nr_inodes = 0, nr_free_inodes = 0;

static inline int const hashfn(dev_t dev, unsigned int i) {
	return (dev ^ i) % NR_IHASH;
}

static inline struct inode_hash_entry * const hash(dev_t dev, int i) {
    return hash_table + hashfn(dev, i);
}

static void insert_inode_free(struct inode *inode) {
    inode->i_next = first_inode;
    inode->i_prev = first_inode->i_prev;
    inode->i_next->i_prev = inode;
    inode->i_prev->i_next = inode;
    first_inode = inode;
}

static void remove_inode_free(struct inode *inode) {
    if (first_inode == inode)
        first_inode = first_inode->i_next;
    if (inode->i_next)
        inode->i_next->i_prev = inode->i_prev;
    if (inode->i_prev)
        inode->i_prev->i_next = inode->i_next;
    inode->i_next = inode->i_prev = NULL;
}

void insert_inode_hash(struct inode *inode) {
    struct inode_hash_entry *h;
    h = hash(inode->i_dev, inode->i_ino);

    inode->i_hash_next = h->inode;
    inode->i_hash_prev = NULL;
    if (inode->i_hash_next)
        inode->i_hash_next->i_hash_prev = inode;
    h->inode = inode;
}

static void remove_inode_hash(struct inode *inode) {
    struct inode_hash_entry *h;
    h = hash(inode->i_dev, inode->i_ino);

    if (h->inode == inode)
        h->inode = inode->i_hash_next;
    if (inode->i_hash_next)
        inode->i_hash_next->i_hash_prev = inode->i_hash_prev;
    if (inode->i_hash_prev)
        inode->i_hash_prev->i_hash_next = inode->i_hash_next;
    inode->i_hash_prev = inode->i_hash_next = NULL;
}

static void put_last_free(struct inode *inode) {
    remove_inode_free(inode);
    inode->i_prev = first_inode->i_prev;
    inode->i_prev->i_next = inode;
    inode->i_next = first_inode;
    inode->i_next->i_prev = inode;
}

void grow_inodes(void) {
    struct inode *inode;
    int i;

    if (!(inode = (struct inode *) get_free_page(GFP_KERNEL)))
        return;
    i = PAGE_SIZE / sizeof(struct inode);
    nr_inodes += i;
    nr_free_inodes += i;
    
    if (!first_inode) {
        inode->i_next = inode->i_prev = first_inode = inode;
        inode++; i--;
    }
    for (; i; i--)
        insert_inode_free(inode++);
}

unsigned long inode_init(unsigned long start, unsigned long end) {
    memset(hash_table, 0, sizeof(hash_table));
    first_inode = NULL;
    return start;
}

static void __wait_on_inode(struct inode *);

static inline void wait_on_inode(struct inode * inode) {
	if (inode->i_lock)
		__wait_on_inode(inode);
}

static inline void lock_inode(struct inode * inode) {
	wait_on_inode(inode);
	inode->i_lock = 1;
}

static inline void unlock_inode(struct inode * inode) {
	inode->i_lock = 0;
	wake_up(&inode->i_wait);
}

void clear_inode(struct inode * inode) {
    struct wait_queue *wait;
    wait_on_inode(inode);
    remove_inode_hash(inode);
    remove_inode_free(inode);
    wait = ((volatile struct inode *) inode)->i_wait;
    if (inode->i_count)
        nr_free_inodes++;
    memset(inode, 0, sizeof(*inode));
    ((volatile struct inode *) inode)->i_wait = wait;
    insert_inode_free(inode);
}

static void write_inode(struct inode * inode) {
    if (!inode->i_dirt)
        return;
    wait_on_inode(inode);
    if (!inode->i_dirt)
        return;
    if (!inode->i_sb || !inode->i_sb->s_op || !inode->i_sb->s_op->write_inode) {
        inode->i_dirt = 0;
        return;
    }
    inode->i_lock = 1;
    inode->i_sb->s_op->write_inode(inode);
	unlock_inode(inode);
}

static void read_inode(struct inode * inode) {
    lock_inode(inode);
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->read_inode)
		inode->i_sb->s_op->read_inode(inode);
	unlock_inode(inode);
}

int bmap(struct inode * inode, int block) {
    if (inode->i_op && inode->i_op->bmap)
		return inode->i_op->bmap(inode,block);
	return 0;
}

void invalidate_inodes(dev_t dev) {

}

void sync_inodes(dev_t dev) {

}

void iput(struct inode * inode) {
    if (!inode)
        return;
    wait_on_inode(inode);
    if (!inode->i_count) {
		printk("VFS: iput: trying to free free inode\n");
		printk("VFS: device %d/%d, inode %lu, mode=0%07o\n",
			MAJOR(inode->i_rdev), MINOR(inode->i_rdev),
					inode->i_ino, inode->i_mode);
		return;
	}
    if (inode->i_pipe)
        wake_up_interruptible(&PIPE_WAIT(*inode));
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;
        return;
    }
    wake_up(&inode_wait);
    if (inode->i_pipe) {
        unsigned long page = (unsigned long) PIPE_BASE(*inode);
        PIPE_BASE(*inode) = NULL;
        free_page(page);
    }
    if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->put_inode) {
		inode->i_sb->s_op->put_inode(inode);
		if (!inode->i_nlink)
			return;
	}
    if (inode->i_dirt) {
        write_inode(inode);
        wait_on_inode(inode);
        goto repeat;
    }
    inode->i_count--;
    nr_free_inodes++;
    return;
}

struct inode * get_empty_inode(void) {
    struct inode *inode, *best;
    int i;

    if (nr_inodes < NR_INODE && nr_free_inodes < (nr_inodes >> 2))
        grow_inodes();
repeat:
    inode = first_inode;
    best = NULL;
    for (i = 0; i < nr_inodes; inode = inode->i_next, i++) {
        if (!inode->i_count) {
            if (!best)
                best = inode;
            if (!inode->i_dirt && !inode->i_lock) {
                best = inode;
                break;
            }
        }
    }
    if (!best || best->i_dirt || best->i_lock)
        if (nr_inodes < NR_INODE) {
            grow_inodes();
            goto repeat;
        }
    inode = best;
    if (!inode) {
        printk("VFS: No free inodes - contact Linus\n");
		sleep_on(&inode_wait);
		goto repeat;
    }
    if (inode->i_lock) {
        wait_on_inode(inode);
        goto repeat;
    }
    if (inode->i_dirt) {
        write_inode(inode);
        goto repeat;
    }
    if (inode->i_count)
        goto repeat;
    clear_inode(inode);
    inode->i_count = 1;
    inode->i_nlink = 1;
    inode->i_sem.count = 1;
    nr_free_inodes--;
    if (nr_free_inodes < 0) {
		printk ("VFS: get_empty_inode: bad free inode count.\n");
		nr_free_inodes = 0;
	}
    return inode;
}

struct inode * iget(struct super_block * sb, int nr) {
	return __iget(sb, nr, 1);
}

struct inode * __iget(struct super_block * sb, int nr, int crossmntp)
{
	static struct wait_queue * update_wait = NULL;
	struct inode_hash_entry * h;
	struct inode * inode;
	struct inode * empty = NULL;
    if (!sb)
		panic("VFS: iget with sb==NULL");
	h = hash(sb->s_dev, nr);
repeat:
    for (inode = h->inode; inode; inode = inode->i_hash_next)
        if (inode->i_dev == sb->s_dev && inode->i_ino == nr)
            goto found_it;
    if (!empty) {
        h->updating++;
        empty = get_empty_inode();
        if (!--h->updating)
            wake_up(&update_wait);
        if (empty)
            goto repeat;
        return NULL;
    }
    inode = empty;
    inode->i_sb = sb;
    inode->i_dev = sb->s_dev;
    inode->i_ino = nr;
    inode->i_flags = sb->s_flags;
    put_last_free(inode);
    insert_inode_hash(inode);
    read_inode(inode);
    goto return_it;

found_it:
    if (!inode->i_count)
        nr_free_inodes--;
    inode->i_count++;
    wait_on_inode(inode);
    if (inode->i_dev != sb->s_dev || inode->i_ino != nr) {
		printk("Whee.. inode changed from under us. Tell Linus\n");
		iput(inode);
		goto repeat;
	}
    // 如果允许跨设备，就使用挂载点
    if (crossmntp && inode->i_mount) {
        struct inode *tmp = inode->i_mount;
        tmp->i_count++;
        iput(inode);
        inode = tmp;
        wait_on_inode(inode);
    }
    if (empty)
        iput(empty);

return_it:
    while (h->updating)
        sleep_on(&update_wait);
    return inode;
}

static void __wait_on_inode(struct inode * inode) {
	struct wait_queue wait = { current, NULL };

	add_wait_queue(&inode->i_wait, &wait);
repeat:
	current->state = TASK_UNINTERRUPTIBLE;
	if (inode->i_lock) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&inode->i_wait, &wait);
	current->state = TASK_RUNNING;
}