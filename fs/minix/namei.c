#include <linux/sched.h>
#include <linux/minix_fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/errno.h>

#include <asm/segment.h>

static struct buffer_head * minix_find_entry(struct inode * dir,
	const char * name, int namelen, struct minix_dir_entry ** res_dir) {
    unsigned long block, offset;
    struct buffer_head *bh;
    struct minix_sb_info *info;

    *res_dir = NULL;
    if (!dir || !dir->i_sb)
        return NULL;
    info = &dir->i_sb->u.minix_sb;
    if (namelen > info->s_namelen) {
#ifdef NO_TRUNCATE
        return NULL;
#else
        namelen = info->s_namelen;
#endif
    }
    bh = NULL;
    block = offset = 0;
    while (block * BLOCK_SIZE + offset < dir->i_size) {
        if (!bh) {
            bh = minix_bread(dir, block, 0);
            if (!bh) {
                block++;
                continue;
            }
        }
        *res_dir = (struct minix_dir_entry *) (bh->b_data + offset);
        if (minix_match(namelen, name, bh, &offset, info))
            return bh;
        if (offset < bh->b_size)
            continue;
        brelse(bh);
        bh = NULL;
        offset = 0;
        block++;
    }
    brelse(bh);
    *res_dir = NULL;
    return NULL;
}

int minix_lookup(struct inode *dir, const char *name, int len,
    struct inode ** result) {
    int ino;
    struct minix_dir_entry *de;
    struct buffer_head *bh;
    *result = NULL;
    if (!dir)
        return -ENOENT;
    if (!S_ISDIR(dir->i_mode)) {
        iput(dir);
        return -ENOENT;
    }
    if (!(bh = minix_find_entry(dir, name, len, &de))) {
        iput(dir);
        return -ENOENT;
    }
    ino = de->inode;
    brelse(bh);
    if (!(result = iget(dir->i_sb, ino))) {
        iput(dir);
        return -EACCES;
    }
    iput(dir);
    return 0;
}

int minix_create(struct inode * dir,const char * name, int len, int mode,
	struct inode ** result) {
        return 0;
    }

int minix_mknod(struct inode * dir, const char * name, int len, int mode, int rdev) {
    return 0;
}

int minix_mkdir(struct inode * dir, const char * name, int len, int mode) {
    return 0;
}

int minix_rmdir(struct inode * dir, const char * name, int len) {
    return 0;
}

int minix_unlink(struct inode * dir, const char * name, int len) {
    return 0;
}

int minix_symlink(struct inode * dir, const char * name, int len, const char * symname) {
    return 0;
}

int minix_link(struct inode * oldinode, struct inode * dir, const char * name, int len) {
    return 0;
}

int minix_rename(struct inode * old_dir, const char * old_name, int old_len,
	struct inode * new_dir, const char * new_name, int new_len) {
        return 0;
    }