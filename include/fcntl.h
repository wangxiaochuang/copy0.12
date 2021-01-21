#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

#define O_ACCMODE	00003
#define O_RDONLY	   00
#define O_WRONLY	   01
#define O_RDWR		   02

#define O_CREAT		00100	/* not fcntl */	/* 如果文件不存在就创建 */
#define O_EXCL		00200	/* not fcntl */	/* 独占使用文件标志 */
#define O_NOCTTY	00400	/* not fcntl */	/* 不分配控制终端 */
#define O_TRUNC		01000	/* not fcntl */	/* 若文件已存在且是写操作，则长度截为0 */
#define O_APPEND	02000					/* 以追加方式打开,文件指针置为文件尾 */
#define O_NONBLOCK	04000	/* not fcntl */	/* 非阻塞方式打开和操作文件 */
#define O_NDELAY	O_NONBLOCK				/* 阻塞方式打开和操作文件 */

extern int open(const char * filename, int flags, ...);

#endif