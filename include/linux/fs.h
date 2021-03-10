#ifndef _FS_H
#define _FS_H

#include <sys/types.h>
#include <sys/dirent.h>

/* 判断设备是否是可以寻找定位的 */
#define IS_SEEKABLE(x) ((x) >= 1 && (x) <= 3)

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define READ 	0
#define WRITE 	1
#define READA 	2
#define WRITEA 	3

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a)) >> 8)		/* 取高字节(主设备号) */
#define MINOR(a) ((a) & 0xff)				/* 取低字节(次设备号) */

#define NR_OPEN 		20
#define NR_INODE 		128
#define NR_FILE 		64
#define NR_SUPER 		8
#define NR_HASH 		307
#define NR_BUFFERS 		nr_buffers
#define BLOCK_SIZE 		1024
#define BLOCK_SIZE_BITS 10
#ifndef NULL
	#define NULL ((void *) 0)
#endif

#define PIPE_READ_WAIT(inode) 	((inode).i_wait)
#define PIPE_WRITE_WAIT(inode) 	((inode).i_wait2)
#define PIPE_HEAD(inode) 		((inode).i_data[0])		/* 管道头部指针 */
#define PIPE_TAIL(inode) 		((inode).i_data[1])		/* 管道尾部指针 */
#define PIPE_SIZE(inode)		((PIPE_HEAD(inode) - PIPE_TAIL(inode)) & (PAGE_SIZE - 1))	/* 管道大小 */
#define PIPE_EMPTY(inode) 		(PIPE_HEAD(inode) == PIPE_TAIL(inode))	/* 管道空 */
#define PIPE_FULL(inode) 		(PIPE_SIZE(inode) == (PAGE_SIZE - 1))	/* 管道满 */

#define NIL_FILP	((struct file *)0)
#define SEL_IN		1
#define SEL_OUT		2
#define SEL_EX		4

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

struct inode {
	dev_t	i_dev;
	ino_t	i_ino;
	umode_t	i_mode;
	nlink_t	i_nlink;
	uid_t	i_uid;
	gid_t	i_gid;
	dev_t	i_rdev;
	off_t	i_size;
	time_t	i_atime;
	time_t	i_mtime;
	time_t	i_ctime;
	unsigned long i_data[16];
	struct inode_operations * i_op;
	struct super_block * i_sb;
	struct task_struct * i_wait;
	struct task_struct * i_wait2;	/* for pipes */
	unsigned short i_count;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_mount;
	unsigned char i_seek;
	unsigned char i_update;
};

/* 文件结构(用于在文件句柄与i节点之间建立关系) */
struct file {
	unsigned short f_mode;				/* 文件操作模式(RW位) */
	unsigned short f_flags;				/* 文件打开和控制的标志 */
	unsigned short f_count;				/* 对应文件引用计数值 */
	struct inode *f_inode;			/* 指向对应i节点 */
	struct file_operations *f_op;
	off_t f_pos;						/* 文件位置(读写偏移值) */
};

struct super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
/* These are only in memory */
	struct buffer_head * s_imap[8];
	struct buffer_head * s_zmap[8];
	unsigned short s_dev;
	struct inode * s_covered;			// 挂载后目录的inode
	struct inode * s_mounted;			// 挂载前目录的inode
	unsigned long s_time;
	struct task_struct * s_wait;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
	/* TUBE */
	struct super_operations *s_op;
};

struct file_operations {
	int (*lseek) (struct inode *, struct file *, off_t, int);
	int (*read) (struct inode *, struct file *, char *, int);
	int (*write) (struct inode *, struct file *, char *, int);
	int (*readdir) (struct inode *, struct file *, struct dirent *);
};

struct inode_operations {
	int (*create) (struct inode *,const char *,int,int,struct inode **);
	int (*lookup) (struct inode *,const char *,int,struct inode **);
	int (*link) (struct inode *,struct inode *,const char *,int);
	int (*unlink) (struct inode *,const char *,int);
	int (*symlink) (struct inode *,const char *,int,const char *);
	int (*mkdir) (struct inode *,const char *,int,int);
	int (*rmdir) (struct inode *,const char *,int);
	int (*mknod) (struct inode *,const char *,int,int,int);
	int (*rename) (struct inode *,const char *,int,struct inode *,const char *,int);
	int (*readlink) (struct inode *,char *,int);
	int (*open) (struct inode *, struct file *);
	void (*release) (struct inode *, struct file *);
	struct inode * (*follow_link) (struct inode *, struct inode *);
	int (*bmap) (struct inode *,int);
	void (*truncate) (struct inode *);
	/* added by entropy */
	void (*write_inode)(struct inode *inode);
	void (*put_inode)(struct inode *inode);
};

struct super_operations {
	void (*read_inode)(struct inode *inode);
	void (*put_super)(struct super_block *sb);
};

struct file_system_type {
	struct super_block *(*read_super)(struct super_block *sb, void *mode);
	char *name;
};

extern struct file_system_type *get_fs_type(char *name);

extern struct inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern int nr_buffers;

/**** 磁盘操作函数原型 ****/
/* 检测驱动器中软盘是否改变 */
extern void check_disk_change(int dev);

/* 检测指定软驱中软盘更换情况 */
extern int floppy_change(unsigned int nr);

/* 将i节点指定的文件截为0 */
extern void truncate(struct inode * inode);

/* 刷新i节点信息 */
extern void sync_inodes(void);

/* 等待指定的i节点 */
extern void wait_on(struct inode * inode);

/* 逻辑块（区段，磁盘块）位图操作。*/
extern int bmap(struct inode * inode, int block);

/* 创建数据块block在设备上对应的逻辑块 */
extern int create_block(struct inode * inode, int block);

/* 获取指定路径名的i节点号 */
extern struct inode * namei(const char * pathname);

/* 取指定路径名的i节点，不跟随符号链接 */
extern struct inode * lnamei(const char * pathname);
extern int permission(struct inode * inode,int mask);
extern struct inode * _namei(const char *filename, struct inode *base,
	int follow_links);

/* 根据路径名为打开文件操作作准备 */
extern int open_namei(const char * pathname, int flag, int mode, 
						struct inode ** res_inode);

/* 释放一个i节点（回写入设备）*/
extern void iput(struct inode * inode);

/* 从设备读取指定节点号的一个i节点 */
extern struct inode * iget(int dev, int nr);

/* 从i节点表中获取一个空闲i节点项 */
extern struct inode * get_empty_inode(void);

/* 获取(申请)管道节点 */
extern struct inode * get_pipe_inode(void);

/* 在哈希表中查找指定的数据块 */
extern struct buffer_head * get_hash_table(int dev, int block);

/* 从设备读取指定块 */
extern struct buffer_head * getblk(int dev, int block);

extern void ll_rw_block(int rw, struct buffer_head * bh);
/* 读/写数据页面 */
extern void ll_rw_page(int rw, int dev, int nr, char * buffer);

extern void ll_rw_swap_file(int rw, int dev, unsigned int *b, int nb, char *buffer);

extern void brelse(struct buffer_head * buf);

extern struct buffer_head * bread(int dev, int block);

/* 读取设备上一个页面(4个缓冲块)的内容到指定内存地址处 */
extern void bread_page(unsigned long addr, int dev, int b[4]);

/* 读取头一个指定的数据块，并标记后续将要读的块 */
extern struct buffer_head * breada(int dev, int block, ...);

/* 向设备dev申请一个磁盘块 */
extern int new_block(int dev);

/* 释放设备数据区中的逻辑块 */
extern int free_block(int dev, int block);

/* 为设备dev建立一个新i节点 */
extern struct inode * new_inode(int dev);

/* 释放一个i节点 */
extern void free_inode(struct inode * inode);

/* 刷新指定设备缓冲区块 */
extern int sync_dev(int dev);

/* 读取指定设备的超级块 */
extern struct super_block * get_super(int dev);

/* 释放指定设备的超级块 */
extern void put_super(int dev);

/* 释放设备dev在内存i节点表中的所有i节点 */
extern void invalidate_inodes(int dev);

extern int ROOT_DEV;

extern void mount_root(void);
extern void lock_super(struct super_block * sb);
extern void free_super(struct super_block * sb);

extern int pipe_read(struct inode *, struct file *, char *, int);
extern int char_read(struct inode *, struct file *, char *, int);
extern int block_read(struct inode *, struct file *, char *, int);

extern int pipe_write(struct inode *, struct file *, char *, int);
extern int char_write(struct inode *, struct file *, char *, int);
extern int block_write(struct inode *, struct file *, char *, int);
#endif