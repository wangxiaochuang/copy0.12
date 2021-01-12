#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

#define NR_OPEN 		20

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

#endif