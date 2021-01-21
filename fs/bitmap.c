#include <string.h>
#include <linux/sched.h>
#include <linux/kernel.h>

/* 将指定地址(addr)处的一块1024字节内存清零 */
#define clear_block(addr) 												\
	__asm__(															\
		"cld\n\t"         												\
		"rep\n\t" 														\
		"stosl" 														\
		::"a" (0), "c" (BLOCK_SIZE / 4), "D" ((long) (addr)))

#define set_bit(nr, addr) ({											\
	register int res; 													\
	__asm__ __volatile__("btsl %2, %3\n\tsetb %%al"						\
				:"=a" (res)												\
				:"0" (0),"r" (nr),"m" (*(addr))); 						\
		res;})

#define clear_bit(nr, addr) ({											\
	register int res;													\
	__asm__ __volatile__("btrl %2, %3\n\tsetnb %%al"					\
			:"=a" (res) 												\
			:"0" (0), "r" (nr), "m" (*(addr))); 						\
		res;})

void free_inode(struct m_inode * inode) {
    struct super_block *sb;
    struct buffer_head *bh;

    if (!inode) {
        return;
    }
    if (!inode->i_dev) {
        memset(inode, 0, sizeof(*inode));
    }
    if (inode->i_count > 1) {
        panic("trying to free inode with count=%d\n", inode->i_count);
    }
    if (inode->i_nlinks) {
        panic("trying to free inode with links");
    }
    if (!(sb = get_super(inode->i_dev))) {
        panic("trying to free inode on nonexistent device");
    }
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes) {
        panic("trying to free inode 0 or nonexistant inode");
    }
    if (!(bh = sb->s_imap[inode->i_num >> 13])) {
        panic("nonexistent imap in superblock");
    }
    if (clear_bit(inode->i_num & 8191, bh->b_data)) {
        printk("free_inode: bit already cleared.\n\r");
    }
    bh->b_dirt = 1;
    memset(inode, 0, sizeof(*inode));
}