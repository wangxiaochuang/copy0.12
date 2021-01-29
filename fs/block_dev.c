#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

// 设备号确定的一个子设备上所拥有的数据总数(1块大小 = 1KB).
extern int *blk_size[];

int block_read(int dev, unsigned long * pos, char * buf, int count) {
    int block = *pos >> BLOCK_SIZE_BITS;
    int offset = *pos & (BLOCK_SIZE-1);
    int chars;
    int size;
    int read = 0;
    struct buffer_head *bh;
    register char *p;

    if (blk_size[MAJOR(dev)]) {
        size = blk_size[MAJOR(dev)][MINOR(dev)];
    } else {
        size = 0x7fffffff;
    }
    while (count > 0) {
        // 要读的块号肯定不能大于总块数
        if (block >= size) {
            return read ? read : -EIO;
        }
        chars = BLOCK_SIZE - offset;
        if (chars > count) {
            chars = count;
        }
        if (!(bh = breada(dev, block, block+1, block+2, -1))) {
            return read ? read : -EIO;
        }
        block++;
    }
}