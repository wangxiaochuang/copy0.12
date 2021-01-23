#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

/* 判断设备是否是可以寻找定位的 */
#define IS_SEEKABLE(x) ((x) >= 1 && (x) <= 3)

#define READ 	0
#define WRITE 	1
#define READA 	2
#define WRITEA 	3

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a)) >> 8)		/* 取高字节(主设备号) */
#define MINOR(a) ((a) & 0xff)				/* 取低字节(次设备号) */

#define NAME_LEN 		14
#define ROOT_INO 		1

#define I_MAP_SLOTS 	8					/* i节点位图的块数 */
#define Z_MAP_SLOTS 	8					/* 逻辑块(区段块)位图的块数 */
#define SUPER_MAGIC 	0x137F		/* MINIX文件系统魔数 */

#define NR_OPEN 		20
#define NR_INODE 		64
#define NR_FILE 		64
#define NR_SUPER 		8
#define NR_HASH 		307
#define NR_BUFFERS 		nr_buffers
#define BLOCK_SIZE 		1024
#define BLOCK_SIZE_BITS 10
#ifndef NULL
	#define NULL ((void *) 0)
#endif

/* 每个逻辑块可存放的i节点数 */
#define INODES_PER_BLOCK ((BLOCK_SIZE) / (sizeof (struct d_inode)))
/* 每个逻辑块可存放的目录数 */
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE) / (sizeof (struct dir_entry)))

/* 缓冲块头数据结构(重要) */
struct buffer_head {
	char * b_data;						/* pointer to data block (1024 bytes) */	
										/* 数据指针 */
	unsigned long b_blocknr;			/* block number */
										/* 块号 */
	unsigned short b_dev;				/* device (0 = free) */
										/* 数据源的设备号 */
	unsigned char b_uptodate;       	/* 更新标志：表示数据是否已更新 */

	unsigned char b_dirt;				/* 0-clean, 1-dirty */	
										/* 修改标志：0未修改，1已修改 */
	unsigned char b_count;				/* users using this block */
										/* 使用用户数 */
	unsigned char b_lock;				/* 0 - ok, 1 -locked */	
										/* 缓冲区是否被锁定 */
	struct task_struct * b_wait;		/* 指向等待该缓冲区解锁的任务 */

	/* 这四个指针用于缓冲区的管理 */
	struct buffer_head * b_prev;		/* hash队列上的前一块 */
	struct buffer_head * b_next;		/* hash队列上的后一块 */
	struct buffer_head * b_prev_free;	/* 空闲表上的前一块 */
	struct buffer_head * b_next_free;	/* 空闲表上的后一块 */
};

/* 磁盘上的索引节点(i节点)数据结构 */
struct d_inode {
	unsigned short i_mode;				/* 文件类型和属性(rwx位) */
	unsigned short i_uid;				/* 用户id(文件拥有者标识符) */
	unsigned long i_size;				/* 文件大小(字节数) */
	unsigned long i_time;				/* 修改时间(自1970.1.1.:0算起，秒) */
	unsigned char i_gid;				/* 组id(文件拥有者所在的组) */
	unsigned char i_nlinks;				/* 链接数(多少个文件目录项指向该i节点) */
	unsigned short i_zone[9];			/* 直接(0-6)，间接(7)或双重间接(8)逻辑块号 */
										/* zone是区的意思，可译成区段，或逻辑块 */
};

/* 内存中的索引节点(i节点)数据结构 */
struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
	/* these are in memory also */		/* 以下是内存中特有的 */
	struct task_struct * i_wait;		/* 等待该i节点的进程 */
	struct task_struct * i_wait2;		/* for pipes */
	unsigned long i_atime;				/* 最后访问时间 */
	unsigned long i_ctime;				/* i节点自身修改时间 */
	unsigned short i_dev;				/* i节点所在的设备号 */
	unsigned short i_num;				/* i节点号 */
	unsigned short i_count;				/* i节点被使用的次数，0表示该i节点空闲 */
	unsigned char i_lock;				/* 锁定标志 */
	unsigned char i_dirt;				/* 已修改(脏)标志 */
	unsigned char i_pipe;				/* 管道标志 */
	unsigned char i_mount;				/* 安装标志 */
	unsigned char i_seek;				/* 搜寻标志(lseek时) */
	unsigned char i_update;				/* 更新标志 */
};

/* 文件结构(用于在文件句柄与i节点之间建立关系) */
struct file {
	unsigned short f_mode;				/* 文件操作模式(RW位) */
	unsigned short f_flags;				/* 文件打开和控制的标志 */
	unsigned short f_count;				/* 对应文件引用计数值 */
	struct m_inode *f_inode;			/* 指向对应i节点 */
	off_t f_pos;						/* 文件位置(读写偏移值) */
};

/* 内存中的超级块结构 */
struct super_block {
	unsigned short s_ninodes;			/* 节点数 */
	unsigned short s_nzones;			/* 逻辑块数 */
	unsigned short s_imap_blocks;		/* i节点位图所占用的数据块数 */
	unsigned short s_zmap_blocks;		/* 逻辑块位图所占用的数据块数 */
	unsigned short s_firstdatazone;		/* 第一个数据逻辑块号 */
	unsigned short s_log_zone_size;		/* log2(数据块数/逻辑块) */
	unsigned long s_max_size;			/* 文件最大长度 */
	unsigned short s_magic;				/* 文件系统魔数 */
	/* These are only in memory */		/* 以下是内存中特有的 */
	struct buffer_head * s_imap[8];		/* i节点位图缓冲块指针数组(占用8块，可表示64M) */
	struct buffer_head * s_zmap[8];		/* 逻辑块位图缓冲块指针数组(占用8块) */
	unsigned short s_dev;				/* 超级块所在设备号 */
	struct m_inode * s_isup;			/* 被安装的文件系统根目录的i节点(isup-superi) */
	struct m_inode * s_imount;			/* 被安装到的i节点 */
	unsigned long s_time;				/* 修改时间 */
	struct task_struct * s_wait;		/* 等待该超级块的进程 */
	unsigned char s_lock;				/* 被锁定标志 */
	unsigned char s_rd_only;			/* 只读标志 */
	unsigned char s_dirt;				/* 已修改(脏)标志 */
};

/* 磁盘上的超级块结构 */
struct d_super_block {
	unsigned short s_ninodes;			/* 节点数 */
	unsigned short s_nzones;			/* 逻辑块数 */
	unsigned short s_imap_blocks;		/* i节点位图所占用的数据块数 */
	unsigned short s_zmap_blocks;		/* 逻辑块位图所占用的数据块数 */
	unsigned short s_firstdatazone;		/* 第一个数据逻辑块号 */
	unsigned short s_log_zone_size;		/* log(数据块数/逻辑块) */
	unsigned long s_max_size;			/* 文件最大长度 */
	unsigned short s_magic;				/* 文件系统魔数 */
};

/* 文件目录项结构 */
struct dir_entry {
	unsigned short inode;				/* i节点号 */
	char name[NAME_LEN];				/* 文件名，长度NAME_LEN=14 */
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern int nr_buffers;

/* 将i节点指定的文件截为0 */
extern void truncate(struct m_inode * inode);

/* 刷新i节点信息 */
extern void sync_inodes(void);

/* 等待指定的i节点 */
extern void wait_on(struct m_inode * inode);

/* 逻辑块（区段，磁盘块）位图操作。*/
extern int bmap(struct m_inode * inode, int block);

/* 获取指定路径名的i节点号 */
extern struct m_inode * namei(const char * pathname);

/* 取指定路径名的i节点，不跟随符号链接 */
extern struct m_inode * lnamei(const char * pathname);

/* 根据路径名为打开文件操作作准备 */
extern int open_namei(const char * pathname, int flag, int mode, 
						struct m_inode ** res_inode);

/* 释放一个i节点（回写入设备）*/
extern void iput(struct m_inode * inode);

/* 从设备读取指定节点号的一个i节点 */
extern struct m_inode * iget(int dev, int nr);

/* 从i节点表中获取一个空闲i节点项 */
extern struct m_inode * get_empty_inode(void);

extern void ll_rw_block(int rw, struct buffer_head * bh);
/* 读/写数据页面 */
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);

extern void brelse(struct buffer_head * buf);

extern struct buffer_head * bread(int dev, int block);

/* 释放一个i节点 */
extern void free_inode(struct m_inode * inode);

/* 刷新指定设备缓冲区块 */
extern int sync_dev(int dev);

/* 读取指定设备的超级块 */
extern struct super_block * get_super(int dev);

/* 释放指定设备的超级块 */
extern void put_super(int dev);

extern int ROOT_DEV;

extern void mount_root(void);

#endif