#include <asm/segment.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/stat.h>

#define ACC_MODE(x) ("\000\004\002\006"[(x)&O_ACCMODE])

int getname(const char * filename, char **result) {
    int error;
	unsigned long i, page;
	char * tmp, c;

	i = (unsigned long) filename;
    if (!i || i >= TASK_SIZE)
        return -EFAULT;
    i = TASK_SIZE - i;
    error = -EFAULT;
    if (i > PAGE_SIZE) {
        i = PAGE_SIZE;
        error = -ENAMETOOLONG;
    }
    c = get_fs_byte(filename++);
    if (!c)
        return -ENOENT;
    if (!(page = __get_free_page(GFP_KERNEL)))
        return -ENOMEM;
    *result = tmp = (char *) page;
    while (--i) {
        *(tmp++) = c;
        c = get_fs_byte(filename++);
        if (!c) {
            *tmp = '\0';
            return 0;
        }
    }
    free_page(page);
    return error;
}

void putname(char * name) {
	free_page((unsigned long) name);
}

int permission(struct inode * inode,int mask) {
    int mode = inode->i_mode;

    if (inode->i_op && inode->i_op->permission)
        return inode->i_op->permission(inode, mask);
    else if (current->euid == inode->i_uid)
        mode >>= 6;
    else if (in_group_p(inode->i_gid))
        mode >>= 3;
    if (((mode & mask & 0007) == mask) || suser())
        return 1;
    return 0;
}

int lookup(struct inode *dir, const char *name, int len, struct inode **result) {
    struct super_block *sb;
    int perm;

    *result = NULL;
    if (!dir)
        return -ENOENT;
    perm = permission(dir, MAY_EXEC);
    if (len == 2 && name[0] == '.' && name[1] == '.') {
        if (dir == current->root) {
            *result = dir;
            return 0;
        } else if ((sb = dir->i_sb) && (dir == sb->s_mounted)) {
            sb = dir->i_sb;
            iput(dir);
            dir = sb->s_covered;
            if (!dir)
                return -ENOENT;
            dir->i_count++;
        }
    }
    if (!dir->i_op || !dir->i_op->lookup) {
		iput(dir);
		return -ENOTDIR;
	}
 	if (!perm) {
		iput(dir);
		return -EACCES;
	}
	if (!len) {
		*result = dir;
		return 0;
	}
	return dir->i_op->lookup(dir, name, len, result);
}

int follow_link(struct inode * dir, struct inode * inode,
	int flag, int mode, struct inode ** res_inode) {
    if (!dir || !inode) {
        iput(dir);
		iput(inode);
		*res_inode = NULL;
		return -ENOENT;
    }
    if (!inode->i_op || !inode->i_op->follow_link) {
		iput(dir);
		*res_inode = inode;
		return 0;
	}
    return inode->i_op->follow_link(dir, inode, flag, mode, res_inode);
}

// 找到路径pathname的basename放到name和namelen，目录inode用res_inode返回
static int dir_namei(const char * pathname, int * namelen, const char ** name,
	struct inode * base, struct inode ** res_inode) {
    char c;
    const char *thisname;
    int len, error;
    struct inode *inode;

    *res_inode = NULL;
    if (!base) {
        base = current->pwd;
        base->i_count++;
    }
    if ((c = *pathname) == '/') {
        iput(base);
        base = current->root;
        pathname++;
        base->i_count++;
    }
    while (1) {
        thisname = pathname;
        for (len = 0; (c = *(pathname++)) && (c != '/'); len++)
            ;
        if (!c)
            break;
        base->i_count++;
        error = lookup(base, thisname, len, &inode);
        if (error) {
            iput(base);
            return error;
        }
        error = follow_link(base, inode, 0, 0, &base);
        if (error)
            return error;
    }
    if (!base->i_op || !base->i_op->lookup) {
        iput(base);
        return -ENOTDIR;
    }
    *name = thisname;
    *namelen = len;
    *res_inode = base;
    return 0;
}

int open_namei(const char * pathname, int flag, int mode,
	struct inode ** res_inode, struct inode * base) {
    const char *basename;
    int namelen, error;
    struct inode *dir, *inode;
    struct task_struct **p;

    mode &= S_IALLUGO & ~current->umask;
    mode |= S_IFREG;
    error = dir_namei(pathname, &namelen, &basename, base, &dir);
    if (error)
        return error;
    if (!namelen) {
        if (flag & 2) {
            iput(dir);
            return -EISDIR;
        }
        if (!permission(dir, ACC_MODE(flag))) {
            iput(dir);
            return -EACCES;
        }
        *res_inode = dir;
        return 0;
    }
    dir->i_count++;
    if (flag & O_CREAT) {
        down(&dir->i_sem);
        error = lookup(dir, basename, namelen, &inode);
        if (!error) {
            if (flag & O_EXCL) {
                iput(inode);
                error = -EEXIST;
            }
        } else if (!permission(dir, MAY_WRITE | MAY_EXEC))
            error = -EACCES;
        else if (!dir->i_op || !dir->i_op->create)
            error = -EACCES;
        else if (IS_RDONLY(dir))
            error = -EROFS;
        else {
            dir->i_count++;
            error = dir->i_op->create(dir, basename, namelen, mode, res_inode);
            up(&dir->i_sem);
            iput(dir);
            return error;
        }
        up(&dir->i_sem);
    } else
        error = lookup(dir, basename, namelen, &inode);
    if (error) {
        iput(dir);
        return error;
    }
    error = follow_link(dir, inode, flag, mode, &inode);
    if (error)
        return error;
    if (S_ISDIR(inode->i_mode) && (flag & 2)) {
        iput(inode);
        return -EISDIR;
    }
    if (!permission(inode, ACC_MODE(flag))) {
		iput(inode);
		return -EACCES;
	}
    if (S_ISBLK(inode->i_mode) || S_ISCHR(inode->i_mode)) {
		if (IS_NODEV(inode)) {
			iput(inode);
			return -EACCES;
		}
	} else {
		if (IS_RDONLY(inode) && (flag & 2)) {
			iput(inode);
			return -EROFS;
		}
	}
    if ((inode->i_count > 1) && (flag & 2)) {
        for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
            struct vm_area_struct *mpnt;
            if (!*p)
                continue;
            if (inode == (*p)->executable) {
                iput(inode);
                return -ETXTBSY;
            }
            for (mpnt = (*p)->mmap; mpnt; mpnt = mpnt->vm_next) {
                if (mpnt->vm_page_prot & PAGE_RW)
                    continue;
                if (inode == mpnt->vm_inode) {
                    iput(inode);
                    return -ETXTBSY;
                }
            }
        }
    }
    if (flag & O_TRUNC) {
        inode->i_size = 0;
        if (inode->i_op && inode->i_op->truncate)
	           inode->i_op->truncate(inode);
        if ((error = notify_change(NOTIFY_SIZE, inode))) {
            iput(inode);
            return error;
        }
        inode->i_dirt = 1;
    }
    *res_inode = inode;
    return 0;
}