#include <errno.h>
#include <fcntl.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

int file_read(struct m_inode *inode, struct file *filp, char *buf, int count) {
    int left, chars, nr;
    struct buffer_head *bh;

    if ((left = count) <= 0) {
        return 0;
    }
    while (left) {
        // 通过相对逻辑块号，获得绝对的逻辑块号
        if ((nr = bmap(inode, (filp->f_pos) / BLOCK_SIZE))) {
            if (!(bh = bread(inode->i_dev, nr))) {
                break;
            }
        } else {
            bh = NULL;
        }
        // 当前位置在数据块中的偏移 
        nr = filp->f_pos % BLOCK_SIZE;
        // 当前最多只读到当前块结束
        chars = MIN(BLOCK_SIZE - nr, left);
        filp->f_pos += chars;
        left -= chars;
        if (bh) {
            char *p = nr + bh->b_data;
            while (chars-- > 0) {
                put_fs_byte(*(p++), buf++);
            }
            brelse(bh);
        } else {
            // 根本就没有数据，还读，就给填充0
            put_fs_byte(0, buf++);
        }
    }
    inode->i_atime = CURRENT_TIME;
    return (count-left) ? (count-left) : -ERROR;
}

int file_write(struct m_inode * inode, struct file * filp, char * buf, int count) {
    off_t pos;
    int block, c;
    struct buffer_head *bh;
    char *p;
    int i = 0;

    if (filp->f_flags & O_APPEND) {
        pos = inode->i_size;
    } else {
        pos = filp->f_pos;
    }
    while (i < count) {
        if (!(block = create_block(inode, pos / BLOCK_SIZE))) {
            break;
        }
        if (!(bh = bread(inode->i_dev, block))) {
            break;
        }
        c = pos % BLOCK_SIZE;
        p = c + bh->b_data;
        bh->b_dirt = 1;
        c = BLOCK_SIZE - c;
        if (c > count - i) {
            c = count - i;
        }
        pos += c;
        if (pos > inode->i_size) {
            inode->i_size = pos;
            inode->i_dirt = 1;
        }
        i += c;
        while (c-- > 0) {
            *(p++) = get_fs_byte(buf++);
        }
        brelse(bh);
    }
    inode->i_mtime = CURRENT_TIME;
    if (!(filp->f_flags & O_APPEND)) {
        filp->f_pos = pos;
        inode->i_ctime = CURRENT_TIME;
    }
    return (i ? i : -1);
}