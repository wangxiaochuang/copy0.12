#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

static struct inode * _namei(const char * filename, struct m_inode * base,
    int follow_links);

#define ACC_MODE(x) ("\004\002\006\377"[(x)&O_ACCMODE])

int permission(struct inode *inode, int mask) {
    int mode = inode->i_mode;

    // 已删除的文件
    if (inode->i_dev && !inode->i_nlink) {
        return 0;
    } else if (current->euid == inode->i_uid) {
        mode >>= 6;
    } else if (in_group_p(inode->i_gid)) {
        mode >>= 3;
    }

    if (((mode & mask & 0007) == mask) || suser()) {
        return 1;
    }
    return 0;
}

int lookup(struct inode *dir, const char *name, int len, struct inode **result) {
    struct super_block *sb;

    *result = NULL;
    // 要查找的是..
    if (len == 2 && get_fs_byte(name) == '.' && get_fs_byte(name + 1) == '.') {
        // 在根下找..那就等同于.
        if (dir == current->root)
            len = 1;
        // 如果dir是一个挂载点，有设备挂载到这里的，那么dir就用挂载的文件系统的inode
        else if ((sb = dir->i_sb) && (dir == sb->s_mounted)) {
            sb = dir->i_sb;
            iput(dir);
            if (dir = sb->s_covered)
                dir->i_count++;
        }
    }
    if (!dir)
        return -ENOENT;
    if (!permission(dir, MAY_EXEC)) {
        iput(dir);
        return -EACCES;
    }
    // 要找的len为0，说明是这种/usr/local/
    if (!len) {
        *result = dir;
        return 0;
    }
    if (!dir->i_op || !dir->i_op->lookup) {
        iput(dir);
        return -ENOENT;
    }
    return dir->i_op->lookup(dir, name, len, result);
}

static int match(int len, const char * name, struct dir_entry * de) {
    register int same __asm__("ax");
    if (!de || !de->inode || len > NAME_LEN) {
        return 0;
    }
    if (!len && (de->name[0]=='.') && (de->name[1]=='\0')) {
        return 1;
    }
    /* 有点取巧，de->name[len] != '\0'说明de->name的长度大于len，长度不匹配 */
    if (len < NAME_LEN && de->name[len]) {
        return 0;
    }
    __asm__(
        "cld\n\t"
        "fs ; repe ; cmpsb\n\t"
        "setz %%al"
        :"=a" (same)
        :"0" (0),"S" ((long) name),"D" ((long) de->name),"c" (len)
        );
    return same;
}

static struct buffer_head * find_entry(struct m_inode ** dir,
    const char * name, int namelen, struct dir_entry ** res_dir) {

    int entries;
    int block, i;
    struct buffer_head * bh;
    struct dir_entry * de;
    struct super_block * sb;

#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    entries = (*dir)->i_size / (sizeof (struct dir_entry));
    *res_dir = NULL;
    if (namelen == 2 && get_fs_byte(name) == '.' && get_fs_byte(name+1) == '.') {
        // 应对这种情况：/.. ==> /.
        if ((*dir) == current->root) {
            namelen = 1;
        } else if ((*dir)->i_num == ROOT_INO) {
            sb = get_super((*dir)->i_dev);
            if (sb->s_imount) {
                iput(*dir);
                (*dir) = sb->s_imount;
                (*dir)->i_count++;
            }
        }
    }
    // 从一级区块开始读
    if (!(block = (*dir)->i_zone[0])) {
        return NULL;
    }
    if (!(bh = bread((*dir)->i_dev, block))) {
        return NULL;
    }
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (i < entries) {
        if ((char *) de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            if (!(block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK)) ||
                !(bh = bread((*dir)->i_dev, block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        if (match(namelen, name, de)) {
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}

/**
 * 向目录添加一个条目，返回该条目所在块的缓冲区
 **/
static struct buffer_head * add_entry(struct m_inode * dir,
    const char * name, int namelen, struct dir_entry ** res_dir) {

    int block, i;
    struct buffer_head * bh;
    struct dir_entry * de;

    *res_dir = NULL;
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    if (!namelen) {
        return NULL;
    }
    if (!(block = dir->i_zone[0])) {
        return NULL;
    }
    if (!(bh = bread(dir->i_dev, block))) {
        return NULL;
    }
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (1) {
        if ((char *) de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            // 找到dir下该i目录项对应的块，没有就新申请一个块
            block = create_block(dir, i/DIR_ENTRIES_PER_BLOCK);
            if (!block)
                return NULL;
            // 如果读取的磁盘逻辑块数据返回的缓冲块指针为空，说明这块逻辑块可能是因不存在而新创建的空块
            // 加上一个块容纳的目录项数，用以跳过这个新块
            // ? bh都为NULL，continue回去要报错把？测试一下 @todo
            if (!(bh = bread(dir->i_dev, block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        if (i * sizeof(struct dir_entry) >= dir->i_size) {
            de->inode = 0;
            dir->i_size = (i + 1) * sizeof(struct dir_entry);
            dir->i_dirt = 1;
            dir->i_ctime = CURRENT_TIME;
        }
        // 为0说明找到一个坑了
        if (!de->inode) {
            dir->i_mtime = CURRENT_TIME;
            for (i = 0; i < NAME_LEN; i++)
                de->name[i] = (i < namelen) ? get_fs_byte(name + i) : 0;
            bh->b_dirt = 1;
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}

struct inode * follow_link(struct inode * dir, struct inode * inode) {
    if (!dir || !inode) {
        iput(dir);
        iput(inode);
        return NULL;
    }
    if (!inode->i_op || !inode->i_op->follow_link) {
        iput(dir);
        return inode;
    }
    return inode->i_op->follow_link(dir, inode);
}

/**
 * 获取目录的inode，如 
 *  /usr/local => inode of /usr, name: local, namelen: 5
 **/
static struct inode * dir_namei(const char * pathname,
    int * namelen, const char ** name, struct inode * base) {

    char c;
    const char *thisname;
    int len, error;
    struct inode *inode;

    if (!base) {
        base = current->pwd;
        base->i_count++;
    }
    // 最后basename为最后一个name
    while ((c = get_fs_byte(pathname)) == '/') {
        iput(base);
        base = current->root;
        pathname++;
        base->i_count++;
    }
    while (1) {
        thisname = pathname;
        for (len = 0; (c = get_fs_byte(pathname++)) && (c != '/'); len++);
        if (!c)
            break;
        base->i_count++;
        error = lookup(base, thisname, len, &inode);
        if (error) {
            iput(base);
            return NULL;
        }
        if (!(base = follow_link(base, inode)))
            return NULL;
    }
    *name = thisname;
    *namelen = len;
    return base;
}

struct inode * _namei(const char * pathname, struct inode * base,
    int follow_links) {
    const char * basename;
    int namelen, error;
    struct inode * inode;

    if (!(base = dir_namei(pathname, &namelen, &basename, base)))
        return NULL;
    base->i_count++;
    error = lookup(base, basename, namelen, &inode);
    if (error) {
        iput(base);
        return NULL;
    }
    if (follow_links)
        inode = follow_link(base, inode);
    else
        iput(base);
    if (inode) {
        inode->i_atime = CURRENT_TIME;
        inode->i_dirt = 1;
    }
    return inode;
}

/**
 * 取指定路径名的i节点，不跟随符号链接
 * @param[in]	pathname	路径名
 * @retval		成功返回对应的i节点，失败返回NULL
*/
struct m_inode * lnamei(const char * pathname) {
    return _namei(pathname, NULL, 0);
}

/**
 * 取指定路径名的i节点,跟随符号链接
 * @param[in]	pathname	路径名
 * @retval		成功返回对应的i节点，失败返回NULL
 */
struct inode * namei(const char * pathname) {
    return _namei(pathname, NULL, 1);
}

int open_namei(const char * pathname, int flag, int mode,
    struct inode ** res_inode) {

    const char *basename;
    int namelen, error;
    struct inode *dir, *inode;

    if ((flag & O_TRUNC) && !(flag & O_ACCMODE)) {
        flag |= O_WRONLY;
    }
    mode &= 0777 & ~current->umask;
    mode |= I_REGULAR;
    if (!(dir = dir_namei(pathname, &namelen, &basename, NULL))) {
        return -ENOENT;
    }
    if (!namelen) {
        if (!(flag & (O_ACCMODE|O_CREAT|O_TRUNC))) {
            *res_inode = dir;
            return 0;
        }
        iput(dir);
        return -EISDIR;
    }
    bh = find_entry(&dir, basename, namelen, &de);
    if (!bh) {
        if (!flag & O_CREAT) {
            iput(dir);
            return -ENOENT;
        }
        if (!permission(dir, MAY_WRITE)) {
            iput(dir);
            return -EACCES;
        }
        inode = new_inode(dir->i_dev);
        if (!inode) {
            iput(dir);
            return -ENOSPC;
        }
        inode->i_uid = current->euid;
        inode->i_mode = mode;
        inode->i_dirt = 1;
        bh = add_entry(dir, basename, namelen, &de);
        if (!bh) {
            inode->i_nlinks--;
            iput(inode);
            iput(dir);
            return -ENOSPC;
        }
        de->inode = inode->i_num;
        bh->b_dirt = 1;
        brelse(bh);
        iput(dir);
        *res_inode = inode;
        return 0;
    }
    inr = de->inode;
    dev = dir->i_dev;
    brelse(bh);
    if (flag & O_EXCL) {
        iput(dir);
        return -EEXIST;
    }
    if (!(inode = follow_link(dir, iget(dev, inr)))) {
        return -EACCES;
    }
    if ((S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)) ||
        !permission(inode, ACC_MODE(flag))) {
        iput(inode);
        return -EPERM;
    }
    inode->i_atime = CURRENT_TIME;
    if (flag & O_TRUNC) {
        truncate(inode);
    }
    *res_inode = inode;
    return 0;
}

/**
 * 创建一个特殊设备文件
 **/
int sys_mknod(const char * filename, int mode, int dev) {
    const char *basename;
    int namelen;
    struct inode *dir;

    if (!suser()) return -EPERM;
    if (!(dir = dir_namei(filename, &namelen, &basename, NULL)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EACCES;
    }
    if (!dir->i_op || !dir->i_op->mknod) {
		iput(dir);
		return -EPERM;
	}
    return dir->i_op->mknod(dir, basename, namelen, mode, dev);
}

int sys_mkdir(const char * pathname, int mode) {
    const char * basename;
    int namelen;
    struct inode * dir;

    if (!(dir = dir_namei(pathname, &namelen, &basename, NULL))) {
        return -ENOENT;
    }
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EPERM;
    }
    if (!dir->i_op || !dir->i_op->mkdir) {
		iput(dir);
		return -EPERM;
	}
	return dir->i_op->mkdir(dir,basename,namelen,mode);
}

/**
 * 检查目录是否为空，是返回1
 **/
static int empty_dir(struct m_inode * inode) {
    int nr,block;
    int len;
    struct buffer_head * bh;
    struct dir_entry * de;

    len = inode->i_size / sizeof (struct dir_entry);
    if (len < 2 || !inode->i_zone[0] ||
        !(bh = bread(inode->i_dev, inode->i_zone[0]))) {
        printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return 0;
    }
    de = (struct dir_entry *) bh->b_data;
    if (de[0].inode != inode->i_num || !de[1].inode ||
        strcmp(".", de[0].name) || strcmp("..", de[1].name)) {
        printk("warning - bad directory on dev %04x\n",inode->i_dev);
        return 0;
    }
    nr = 2;
    de += 2;
    while (nr < len) {
        if ((void *) de >= (void *) (bh->b_data + BLOCK_SIZE)) {
            brelse(bh);
            block = bmap(inode, nr / DIR_ENTRIES_PER_BLOCK);
            if (!block) {
                nr == DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            if (!(bh = bread(inode->i_dev, block))) {
                return 0;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        if (de->inode) {
            brelse(bh);
            return 0;
        }
        de++;
        nr++;
    }
    brelse(bh);
    return 1;
}

int sys_rmdir(const char * name) {
    const char *basename;
    int namelen;
    struct inode *dir;

    if (!(dir = dir_namei(name, &namelen, &basename, NULL)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -ENOENT;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EACCES;
    }
    if (!dir->i_op || !dir->i_op->rmdir) {
        iput(dir);
        return -EPERM;
    }
    return dir->i_op->rmdir(dir, basename, namelen);
}

int sys_unlink(const char * name) {
    const char *basename;
    int namelen;
    struct inode *dir;

    if (!(dir = dir_namei(name, &namelen, &basename, NULL)))
        return -ENOENT;
    if (!namelen) {
        iput(dir);
        return -EPERM;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EACCES;
    }
    if (!dir->i_op || !dir->i_op->unlink) {
        iput(dir);
        return -EPERM;
    }
    return dir->i_op->unlink(dir, basename, namelen);
}

int sys_symlink(const char * oldname, const char * newname) {
    struct inode *dir;
    const char *basename;
    int namelen;

    dir = dir_namei(newname, &namelen, &basename, NULL);
    if (!dir) return -EACCES;
    if (!namelen) {
        iput(dir);
        return -EACCES;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        return -EACCES;
    }
    if (!dir->i_op || !dir->i_op->symlink) {
        iput(dir);
        return -EPERM;
    }
    return dir->i_op->symlink(dir, basename, namelen, oldname);
}

int sys_link(const char * oldname, const char * newname) {
    struct node *oldinode, *dir;
    const char *basename;
    int namelen;

    oldinode = namei(oldname);
    if (!oldinode)
        return -ENOENT;
    dir = dir_namei(newname, &namelen, &basename, NULL);
    if (!dir) {
        iput(oldinode);
        return -EACCES;
    }
    // namelen为0说明是目录，像这样的 /usr/local/mnt/
    if (!namelen) {
        iput(oldinode);
        iput(dir);
        return -EPERM;
    }
    // 硬链接需要是同一个文件系统
    if (dir->i_dev != oldinode->i_dev) {
        iput(dir);
        iput(oldinode);
        return -EXDEV;
    }
    if (!permission(dir, MAY_WRITE)) {
        iput(dir);
        iput(oldinode);
        return -EACCES;
    }
    if (!dir->i_op || !dir->i_op->link) {
        iput(dir);
        iput(oldinode);
        return -EPERM;
    }
    return dir->i_op->link(oldinode, dir, basename, namelen);
}

int sys_rename(const char *oldname, const char *newname) {
    struct inode *old_dir, *new_dir;
    const char *old_base, *new_base;
    int old_len, new_len;

    old_dir = dir_namei(oldname, &old_len, &old_base, NULL);
    if (!old_dir) 
        return -ENOENT;
    if (!permission(old_dir, MAY_WRITE)) {
        iput(old_dir);
        return -EACCES;
    }
    // 不能是斜线 也不能是.和..
    if (!old_len || (get_fs_byte(old_base) == '.' && (old_len == 1 || (get_fs_byte(old_base + 1) == '.' && old_len == 2)))) {
        iput(old_dir);
        return -EPERM;
    }
    new_dir = dir_namei(newname, &new_len, &new_base, NULL);
    if (!new_dir) {
        iput(old_dir);
        return -ENOENT;
    }
    if (!permission(new_dir, MAY_WRITE)) {
        iput(old_dir);
        iput(new_dir);
        return -EPERM;
    }
    if (!new_len || (get_fs_byte(new_base) == '.' && (new_len == 1 || (get_fs_byte(new_base + 1) == '.' && new_len = 2)))) {
        iput(old_dir);
        iput(new_dir);
        return -EPERM;
    }
    // 不支持跨设备
    if (new_dir->i_dev != old_dir->i_dev) {
        iput(old_dir);
        iput(new_dir);
        return -EXDEV;
    }
    if (!old_dir->i_op || old_dir->i_op->rename) {
        iput(old_dir);
        iput(new_dir);
        return -EPERM;
    }
    return old_dir->i_op->rename(old_dir, old_base, old_len, new_dir, new_base, new_len);
}