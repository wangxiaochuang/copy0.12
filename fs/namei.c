#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

static struct m_inode * _namei(const char * filename, struct m_inode * base,
    int follow_links);

#define ACC_MODE(x) ("\004\002\006\377"[(x)&O_ACCMODE])

#define MAY_EXEC 	1	/* 可执行 */
#define MAY_WRITE	2	/* 可写 */
#define MAY_READ 	4	/* 可读 */

static int permission(struct m_inode *inode, int mask) {
    int mode = inode->i_mode;

    // 已删除的文件
    if (inode->i_dev && !inode->i_nlinks) {
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

static struct m_inode * follow_link(struct m_inode * dir, struct m_inode * inode) {
    unsigned short fs;
    struct buffer_head * bh;

    if (!dir) {
        dir = current->root;
        dir->i_count ++;
    }
    if (!inode) {
        iput(dir);
        return NULL;
    }
    if (!S_ISLNK(inode->i_mode)) {
        iput(dir);
        return inode;
    }
    // @todo
    __asm__("mov %%fs,%0":"=r" (fs));
    if (fs != 0x17 || !inode->i_zone[0] ||
       !(bh = bread(inode->i_dev, inode->i_zone[0]))) {
        iput(dir);
        iput(inode);
        return NULL;
    }
    iput(inode);
    __asm__("mov %0,%%fs"::"r" ((unsigned short) 0x10));
    inode = _namei(bh->b_data,dir,0);
    __asm__("mov %0,%%fs"::"r" (fs));
    brelse(bh);
    return inode;
}

static struct m_inode * get_dir(const char * pathname, struct m_inode * inode) {
    char c;
    const char * thisname;
    struct buffer_head * bh;
    int namelen,inr;
    struct dir_entry * de;
    struct m_inode * dir;

    if (!inode) {
        inode = current->pwd;
        inode->i_count++;
    }
    if ((c = get_fs_byte(pathname)) == '/') {
        iput(inode);
        inode = current->root;
        pathname++;
        inode->i_count++;
    }
    while (1) {
        thisname = pathname;
        if (!S_ISDIR(inode->i_mode) || !permission(inode, MAY_EXEC)) {
            iput(inode);
            return NULL;
        }
        for (namelen = 0; (c = get_fs_byte(pathname++)) && (c != '/'); namelen++);
        // 如果c==0，说明读到最后一个字符了，否则c就为'/'
        if (!c) {
            return inode;
        }
        // inode指向basedir，de为basedir的dir_entry
        // 只有第一次使用superblock，才会修改inode
        if (!(bh = find_entry(&inode, thisname, namelen, &de))) {
            iput(inode);
            return NULL;
        }
        inr = de->inode;
        brelse(bh);
        dir = inode;
        if (!(inode = iget(dir->i_dev, inr))) {
            iput(dir);
            return NULL;
        }
        if (!(inode = follow_link(dir, inode)))
            return NULL;
    }
}
/**
 * 获取目录的inode，如 
 *  /usr/local => inode of /usr, name: local, namelen: 5
 **/
static struct m_inode * dir_namei(const char * pathname,
    int * namelen, const char ** name, struct m_inode * base) {

    char c;
    const char *basename;
    struct m_inode *dir;

    // 比如：/usr/local ==> local
    if (!(dir = get_dir(pathname, base))) {
        return NULL;
    }
    basename = pathname;
    // 最后basename为最后一个name
    while ((c = get_fs_byte(pathname++))) {
        if (c == '/') {
            basename = pathname;
        }
    }
    // 获取最后一节的名字和长度
    *namelen = pathname - basename - 1;
    *name = basename;
    return dir;
}

struct m_inode * _namei(const char * pathname, struct m_inode * base,
    int follow_links) {
    const char * basename;
    int inr, namelen;
    struct m_inode * inode;
    struct buffer_head * bh;
    struct dir_entry * de;

    if (!(base = dir_namei(pathname, &namelen, &basename, base))) {
        return NULL;
    }
    /**
     * /usr/local  => inode of /usr
     * /usr/       => inode of /usr
     **/
    if (!namelen) {			/* special case: '/usr/' etc */
        return base;
    }
    bh = find_entry(&base, basename, namelen, &de);
    if (!bh) {
        iput(base);
        return NULL;
    }
    inr = de->inode;
    brelse(bh);
    if (!(inode = iget(base->i_dev, inr))) {
        iput(base);
        return NULL;
    }
    if (follow_link) {
        inode = follow_link(base, inode);
    } else {
        iput(base);
    }
    inode->i_atime = CURRENT_TIME;
    inode->i_dirt = 1;
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
struct m_inode * namei(const char * pathname) {
    return _namei(pathname, NULL, 1);
}

int open_namei(const char * pathname, int flag, int mode,
    struct m_inode ** res_inode) {

    const char *basename;
    int inr,dev,namelen;
    struct m_inode * dir, *inode;
    struct buffer_head * bh;
    struct dir_entry * de;

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
        if (!flag &O_CREAT) {
            iput(dir);
            return -ENOENT;
        }
        if (!permission(dir, MAY_WRITE)) {
            iput(dir);
            return -EACCES;
        }
        // @todo
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