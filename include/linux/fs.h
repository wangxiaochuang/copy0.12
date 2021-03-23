#ifndef _LINUX_FS_H
#define _LINUX_FS_H

#include <linux/linkage.h>
#include <linux/wait.h>
#include <linux/types.h>

#undef NR_OPEN
#define NR_OPEN 256

#define NR_INODE 2048	/* this should be bigger than NR_FILE */
#define NR_FILE 1024	/* this can well be larger on a larger system */
#define NR_SUPER 32
#define NR_HASH 997
#define NR_IHASH 131
#define NR_FILE_LOCKS 64
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

extern void buffer_init(void);
extern unsigned long inode_init(unsigned long start, unsigned long end);
extern unsigned long file_table_init(unsigned long start, unsigned long end);

#define MAJOR(a) (int)((unsigned short)(a) >> 8)
#define MINOR(a) (int)((unsigned short)(a) & 0xFF)
#define MKDEV(a,b) ((int)((((a) & 0xff) << 8) | ((b) & 0xff)))

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define MS_RDONLY    1 /* mount read-only */
#define MS_NOSUID    2 /* ignore suid and sgid bits */
#define MS_NODEV     4 /* disallow access to device special files */
#define MS_NOEXEC    8 /* disallow program execution */
#define MS_SYNC     16 /* writes are synced at once */
#define	MS_REMOUNT  32 /* alter flags of a mounted FS */

typedef char buffer_block[BLOCK_SIZE];

struct buffer_head {
	char * b_data;			/* 指向缓存空间 */
	unsigned long b_size;		/* 缓存大小 */
	unsigned long b_blocknr;	/* block number */
	dev_t b_dev;			/* device (0 = free) */
	unsigned short b_count;		/* users using this block */
	unsigned char b_uptodate;
	unsigned char b_dirt;		/* 0-clean,1-dirty */
	unsigned char b_lock;		/* 0 - ok, 1 -locked */
	unsigned char b_req;		/* 0 if the buffer has been invalidated */
	struct wait_queue * b_wait;
	struct buffer_head * b_prev;		/* doubly linked list of hash-queue */
	struct buffer_head * b_next;
	struct buffer_head * b_prev_free;	/* 上一个空闲的buffer_head */
	struct buffer_head * b_next_free;
	struct buffer_head * b_this_page;	/* circular list of buffers in one page */
	struct buffer_head * b_reqnext;		/* request queue */
};

#include <linux/pipe_fs_i.h>
#include <linux/minix_fs_i.h>
#include <linux/ext_fs_i.h>
#include <linux/ext2_fs_i.h>
#include <linux/hpfs_fs_i.h>
#include <linux/msdos_fs_i.h>
#include <linux/iso_fs_i.h>
#include <linux/nfs_fs_i.h>
#include <linux/xia_fs_i.h>
#include <linux/sysv_fs_i.h>

struct inode {
	dev_t		i_dev;
	unsigned long	i_ino;
	umode_t		i_mode;
	nlink_t		i_nlink;
	uid_t		i_uid;
	gid_t		i_gid;
	dev_t		i_rdev;
	off_t		i_size;
	time_t		i_atime;
	time_t		i_mtime;
	time_t		i_ctime;
	unsigned long	i_blksize;
	unsigned long	i_blocks;
	struct semaphore i_sem;
	struct inode_operations * i_op;
	struct super_block * i_sb;
	struct wait_queue * i_wait;
	struct file_lock * i_flock;
	struct vm_area_struct * i_mmap;
	struct inode * i_next, * i_prev;
	struct inode * i_hash_next, * i_hash_prev;
	struct inode * i_bound_to, * i_bound_by;
	struct inode * i_mount;
	struct socket * i_socket;
	unsigned short i_count;
	unsigned short i_flags;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_seek;
	unsigned char i_update;
	union {
		struct pipe_inode_info pipe_i;
		struct minix_inode_info minix_i;
		struct ext_inode_info ext_i;
		struct ext2_inode_info ext2_i;
		struct hpfs_inode_info hpfs_i;
		struct msdos_inode_info msdos_i;
		struct iso_inode_info isofs_i;
		struct nfs_inode_info nfs_i;
		struct xiafs_inode_info xiafs_i;
		struct sysv_inode_info sysv_i;
	} u;
};

struct file {
	mode_t f_mode;
	dev_t f_rdev;			/* needed for /dev/tty */
	off_t f_pos;
	unsigned short f_flags;
	unsigned short f_count;
	unsigned short f_reada;
	struct file *f_next, *f_prev;
	struct inode * f_inode;
	struct file_operations * f_op;
};

struct file_lock {
	struct file_lock *fl_next;	/* singly linked list */
	struct task_struct *fl_owner;	/* NULL if on free list, for sanity checks */
        unsigned int fl_fd;             /* File descriptor for this lock */
	struct wait_queue *fl_wait;
	char fl_type;
	char fl_whence;
	off_t fl_start;
	off_t fl_end;
};

#include <linux/minix_fs_sb.h>
#include <linux/ext_fs_sb.h>
#include <linux/ext2_fs_sb.h>
#include <linux/hpfs_fs_sb.h>
#include <linux/msdos_fs_sb.h>
#include <linux/iso_fs_sb.h>
#include <linux/nfs_fs_sb.h>
#include <linux/xia_fs_sb.h>
#include <linux/sysv_fs_sb.h>

struct super_block {
	dev_t s_dev;
	unsigned long s_blocksize;
	unsigned char s_blocksize_bits;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
	struct super_operations *s_op;
	unsigned long s_flags;
	unsigned long s_magic;
	unsigned long s_time;
	struct inode * s_covered;
	struct inode * s_mounted;
	struct wait_queue * s_wait;
	union {
		struct minix_sb_info minix_sb;
		struct ext_sb_info ext_sb;
		struct ext2_sb_info ext2_sb;
		struct hpfs_sb_info hpfs_sb;
		struct msdos_sb_info msdos_sb;
		struct isofs_sb_info isofs_sb;
		struct nfs_sb_info nfs_sb;
		struct xiafs_sb_info xiafs_sb;
		struct sysv_sb_info sysv_sb;
	} u;
};

struct file_operations {
	int (*lseek) (struct inode *, struct file *, off_t, int);
	int (*read) (struct inode *, struct file *, char *, int);
	int (*write) (struct inode *, struct file *, char *, int);
	int (*readdir) (struct inode *, struct file *, struct dirent *, int);
	int (*select) (struct inode *, struct file *, int, select_table *);
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	int (*mmap) (struct inode *, struct file *, unsigned long, size_t, int, unsigned long);
	int (*open) (struct inode *, struct file *);
	void (*release) (struct inode *, struct file *);
	int (*fsync) (struct inode *, struct file *);
};

struct inode_operations {
	struct file_operations * default_file_ops;
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
	int (*follow_link) (struct inode *,struct inode *,int,int,struct inode **);
	int (*bmap) (struct inode *,int);
	void (*truncate) (struct inode *);
	int (*permission) (struct inode *, int);
};

struct super_operations {
	void (*read_inode) (struct inode *);
	int (*notify_change) (int flags, struct inode *);
	void (*write_inode) (struct inode *);
	void (*put_inode) (struct inode *);
	void (*put_super) (struct super_block *);
	void (*write_super) (struct super_block *);
	void (*statfs) (struct super_block *, struct statfs *);
	int (*remount_fs) (struct super_block *, int *, char *);
};

struct file_system_type {
	struct super_block *(*read_super) (struct super_block *, void *, int);
	char *name;
	int requires_dev;
};

#ifdef __KERNEL__

extern int register_blkdev(unsigned int, const char *, struct file_operations *);
extern int unregister_blkdev(unsigned int major, const char * name);
extern int blkdev_open(struct inode * inode, struct file * filp);
extern struct file_operations def_blk_fops;
extern struct inode_operations blkdev_inode_operations;

extern int register_chrdev(unsigned int, const char *, struct file_operations *);
extern int unregister_chrdev(unsigned int major, const char * name);
extern int chrdev_open(struct inode * inode, struct file * filp);
extern struct file_operations def_chr_fops;
extern struct inode_operations chrdev_inode_operations;

extern int shrink_buffers(unsigned int priority);

extern void ll_rw_block(int rw, int nr, struct buffer_head * bh[]);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(dev_t dev, int block, int size);
extern dev_t ROOT_DEV;

extern void show_buffers(void);
extern void mount_root(void);

extern int char_read(struct inode *, struct file *, char *, int);
extern int block_read(struct inode *, struct file *, char *, int);
extern int read_ahead[];

extern int char_write(struct inode *, struct file *, char *, int);
extern int block_write(struct inode *, struct file *, char *, int);

extern int block_fsync(struct inode *, struct file *);
extern int file_fsync(struct inode *, struct file *);

#endif

#endif