#include <linux/minix_fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/stat.h>
#include <linux/locks.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/bitops.h>

void minix_put_inode(struct inode *inode) {

}

void minix_write_super (struct super_block * sb) {

}

void minix_put_super(struct super_block *sb) {

}

static struct super_operations minix_sops = { 
	minix_read_inode,
	NULL,
	minix_write_inode,
	minix_put_inode,
	minix_put_super,
	minix_write_super,
	minix_statfs,
	minix_remount
};

int minix_remount (struct super_block * sb, int * flags, char * data) {

}

struct super_block *minix_read_super(struct super_block *s, void *data,
				     int silent) {
    struct buffer_head *bh;
    struct minix_super_block *ms;
    int i, dev = s->s_dev, block;

    if (32 != sizeof(struct minix_inode))
        panic("bad i-node size");
    lock_super(s);
    if (!(bh = bread(dev, 1, BLOCK_SIZE))) {
        s->s_dev = 0;
        unlock_super(s);
        printk("MINIX-fs: unable to read superblock\n");
		return NULL;
    }
    ms = (struct minix_super_block *) bh->b_data;
    s->u.minix_sb.s_ms = ms;
    s->u.minix_sb.s_sbh = bh; 
    s->u.minix_sb.s_mount_state = ms->s_state;
    s->s_blocksize = 1024;
    s->s_blocksize_bits = 10;
    s->u.minix_sb.s_ninodes = ms->s_ninodes;
	s->u.minix_sb.s_nzones = ms->s_nzones;
	s->u.minix_sb.s_imap_blocks = ms->s_imap_blocks;
	s->u.minix_sb.s_zmap_blocks = ms->s_zmap_blocks;
    s->u.minix_sb.s_firstdatazone = ms->s_firstdatazone;
	s->u.minix_sb.s_log_zone_size = ms->s_log_zone_size;
	s->u.minix_sb.s_max_size = ms->s_max_size;
    s->s_magic = ms->s_magic;
    if (s->s_magic == MINIX_SUPER_MAGIC) {
        s->u.minix_sb.s_dirsize = 16;
        s->u.minix_sb.s_namelen = 14;
    } else if (s->s_magic == MINIX_SUPER_MAGIC2) {
        s->u.minix_sb.s_dirsize = 32;
		s->u.minix_sb.s_namelen = 30;
    } else {
        s->s_dev = 0;
        unlock_super(s);
        brelse(bh);
        if (!silent)
			printk("VFS: Can't find a minix filesystem on dev 0x%04x.\n", dev);
		return NULL;
    }
    for (i = 0; i < MINIX_I_MAP_SLOTS; i++)
        s->u.minix_sb.s_imap[i] = NULL;
    for (i = 0; i < MINIX_Z_MAP_SLOTS; i++)
        s->u.minix_sb.s_zmap[i] = NULL;
    block = 2;
    for (i = 0; i < s->u.minix_sb.s_imap_blocks; i++)
        if ((s->u.minix_sb.s_imap[i] = bread(dev, block, BLOCK_SIZE)) != NULL)
            block++;
        else
            break;
    for (i = 0; i < s->u.minix_sb.s_zmap_blocks; i++)
        if ((s->u.minix_sb.s_zmap[i] = bread(dev, block, BLOCK_SIZE)) != NULL)
            block++;
        else
            break;
    if (block != 2 + s->u.minix_sb.s_imap_blocks + s->u.minix_sb.s_zmap_blocks) {
        for(i = 0; i < MINIX_I_MAP_SLOTS; i++)
			brelse(s->u.minix_sb.s_imap[i]);
		for(i = 0; i < MINIX_Z_MAP_SLOTS; i++)
			brelse(s->u.minix_sb.s_zmap[i]);
		s->s_dev=0;
		unlock_super(s);
		brelse(bh);
		printk("MINIX-fs: bad superblock or unable to read bitmaps\n");
		return NULL;
    }
    set_bit(0, s->u.minix_sb.s_imap[0]->b_data);
    set_bit(0, s->u.minix_sb.s_zmap[0]->b_data);
    unlock_super(s);
    s->s_dev = dev;
    s->s_op = &minix_sops;
    s->s_mounted = iget(s, MINIX_ROOT_INO);
    if (!s->s_mounted) {
		s->s_dev = 0;
		brelse(bh);
		printk("MINIX-fs: get root inode failed\n");
		return NULL;
	}
    if (!(s->s_flags & MS_RDONLY)) {
        ms->s_state &= ~MINIX_VALID_FS;
        bh->b_dirt = 1;
        s->s_dirt = 1;
    }
    if (!(s->u.minix_sb.s_mount_state & MINIX_VALID_FS))
		printk ("MINIX-fs: mounting unchecked file system, "
			"running fsck is recommended.\n");
 	else if (s->u.minix_sb.s_mount_state & MINIX_ERROR_FS)
		printk ("MINIX-fs: mounting file system with errors, "
			"running fsck is recommended.\n");
	return s;
}

void minix_statfs(struct super_block *sb, struct statfs *buf) {

}

int minix_bmap(struct inode * inode,int block) {
    return 0;
}

struct buffer_head * minix_bread(struct inode * inode, int block, int create) {
    return NULL;
}

void minix_read_inode(struct inode * inode) {

}

void minix_write_inode(struct inode * inode) {

}

int minix_sync_inode(struct inode * inode) {
    return 0;
}