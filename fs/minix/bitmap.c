#include <linux/sched.h>
#include <linux/minix_fs.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/bitops.h>

#define clear_block(addr) \
__asm__("cld\n\t" \
	"rep\n\t" \
	"stosl" \
	: \
	:"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)))

#define find_first_zero(addr) ({ \
int __res; \
__asm__("cld\n" \
	"1:\tlodsl\n\t" \
	"notl %%eax\n\t" \
	"bsfl %%eax,%%edx\n\t" \
	"jne 2f\n\t" \
	"addl $32,%%ecx\n\t" \
	"cmpl $8192,%%ecx\n\t" \
	"jl 1b\n\t" \
	"xorl %%edx,%%edx\n" \
	"2:\taddl %%edx,%%ecx" \
	:"=c" (__res):"0" (0),"S" (addr)); \
__res;})

void minix_free_block(struct super_block * sb, int block) {
    struct buffer_head *bh;
    unsigned int bit, zone;

    if (!sb) {
		printk("trying to free block on nonexistent device\n");
		return;
	}
	if (block < sb->u.minix_sb.s_firstdatazone ||
	    block >= sb->u.minix_sb.s_nzones) {
		printk("trying to free block not in datazone\n");
		return;
	}
    bh = get_hash_table(sb->s_dev, block, BLOCK_SIZE);
    if (bh)
        bh->b_dirt = 0;
    brelse(bh);
    zone = block - sb->u.minix_sb.s_firstdatazone + 1;
    bit = zone & 8191;
    zone >>= 13;
    bh = sb->u.minix_sb.s_zmap[zone];
    if (!bh) {
        printk("minix_free_block: nonexistent bitmap buffer\n");
		return;
    }
    if (!clear_bit(bit, bh->b_data))
        printk("free_block (%04x:%d): bit already cleared\n",sb->s_dev,block);
    bh->b_dirt = 1;
    return;
}

int minix_new_block(struct super_block * sb) {
    struct buffer_head *bh;
    int i, j;

    if (!sb) {
        printk("trying to get new block from nonexistent device\n");
		return 0;
    }
repeat:
    j = 8192;
    for (i = 0; i < 8; i++)
        if ((bh = sb->u.minix_sb.s_zmap[i]) != NULL)
            if ((j = find_first_zero(bh->b_data)) < 8192)
                break;
    if (i >= 8 || !bh || j >= 8192)
        return 0;
    if (set_bit(j, bh->b_data)) {
        printk("new_block: bit already set");
		goto repeat;
    }
    bh->b_dirt = 1;
    // 计算得到空闲块号
    j += i * 8192 + sb->u.minix_sb.s_firstdatazone - 1;
    if (j < sb->u.minix_sb.s_firstdatazone || j >= sb->u.minix_sb.s_nzones)
        return 0;
    if (!(bh = getblk(sb->s_dev, j, BLOCK_SIZE))) {
        printk("new_block: cannot get block");
		return 0;
    }
    clear_block(bh->b_data);
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    brelse(bh);
    return j;
}