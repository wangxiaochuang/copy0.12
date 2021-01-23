#include <errno.h>
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