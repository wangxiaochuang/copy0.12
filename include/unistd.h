#ifndef _UNISTD_H
#define _UNISTD_H

/* 下面符号常数指出符合IEEE标准1003.1实现的版本号，是一个整数值 */
#define _POSIX_VERSION 198808L

/* chown()和fchown()的使用受限于进程的权限 */
#define _POSIX_CHOWN_RESTRICTED	/* only root can do a chown (I think..) */
								/* 只有超级用户可以执行chown（我想..） */
/* 长于(NAME_MAX)的路径名将产生错误，而不会自动截断 */
#define _POSIX_NO_TRUNC			/* no pathname truncation (but see in kernel) */
								/* 路径名不截断（但是请看内核代码）*/
// 下面这个符号将定义成字符值，该值禁止终端对其的处理。	 
// _POSIX_VDISABLE 用于控制终端某些特殊字符的功能。当一个终端termios结构中c_cc[]数组某项
// 字符代码值等于_POSIX_VDISABLE的值时，表示禁止使用相应的特殊字符。
#define _POSIX_VDISABLE '\0'	/* character to disable things like ^C */
								/* 禁止像^C这样的字符 */
/* 系统实现支持作业控制 */
#define _POSIX_JOB_CONTROL

/* 每个进程都有一保存的set-user-ID和一保存的set-group-ID */
#define _POSIX_SAVED_IDS		/* Implemented, for whatever good it is */

#ifndef NULL
#define NULL    ((void *)0)     /* 定义空指针 */
#endif

/* lseek */ 		/* 文件指针重定位 */
// 以下符号常数用于 lseek() 和 fcntl() 函数。
#define SEEK_SET	0	/* 将文件读写指针设置为偏移值 */
#define SEEK_CUR	1	/* 将文件读写指针设置为当前值加上偏移值 */
#define SEEK_END	2	/* 将文件读写指针设置为文件长度加上偏移值 */

#include <sys/stat.h>

#ifdef __LIBRARY__
#define __NR_setup			 0	/* used only by init, to get system going */ 
								/* __NR_setup仅用于初始化，以启动系统 */
#define __NR_exit			 1
#define __NR_fork			 2
#define __NR_read			 3
#define __NR_write			 4
#define __NR_open			 5
#define __NR_close			 6
#define __NR_waitpid		 7
#define __NR_creat			 8
#define __NR_link			 9
#define __NR_unlink			10
#define __NR_execve			11
#define __NR_chdir			12
#define __NR_time			13
#define __NR_mknod			14
#define __NR_chmod			15
#define __NR_chown			16
#define __NR_break			17
#define __NR_stat			18
#define __NR_lseek			19
#define __NR_getpid			20
#define __NR_mount			21
#define __NR_umount			22
#define __NR_setuid			23
#define __NR_getuid			24
#define __NR_stime			25
#define __NR_ptrace			26
#define __NR_alarm			27
#define __NR_fstat			28
#define __NR_pause			29
#define __NR_utime			30
#define __NR_stty			31
#define __NR_gtty			32
#define __NR_access			33
#define __NR_nice			34
#define __NR_ftime			35
#define __NR_sync			36
#define __NR_kill			37
#define __NR_rename			38
#define __NR_mkdir			39
#define __NR_rmdir			40
#define __NR_dup			41
#define __NR_pipe			42
#define __NR_times			43
#define __NR_prof			44
#define __NR_brk			45
#define __NR_setgid			46
#define __NR_getgid			47
#define __NR_signal			48
#define __NR_geteuid		49
#define __NR_getegid		50
#define __NR_acct			51
#define __NR_phys			52
#define __NR_lock			53
#define __NR_ioctl			54
#define __NR_fcntl			55
#define __NR_mpx			56
#define __NR_setpgid		57
#define __NR_ulimit			58
#define __NR_uname			59
#define __NR_umask			60
#define __NR_chroot			61
#define __NR_ustat			62
#define __NR_dup2			63
#define __NR_getppid		64
#define __NR_getpgrp		65
#define __NR_setsid			66
#define __NR_sigaction		67
#define __NR_sgetmask		68
#define __NR_ssetmask		69
#define __NR_setreuid		70
#define __NR_setregid		71
#define __NR_sigsuspend		72
#define __NR_sigpending 	73
#define __NR_sethostname 	74
#define __NR_setrlimit		75
#define __NR_getrlimit		76
#define __NR_getrusage		77
#define __NR_gettimeofday 	78
#define __NR_settimeofday 	79
#define __NR_getgroups		80
#define __NR_setgroups		81
#define __NR_select			82
#define __NR_symlink		83
#define __NR_lstat			84
#define __NR_readlink		85
#define __NR_uselib			86
#define __NR_swapon			87
#define __NR_reboot			88
#define __NR_readdir		89

/**
 * INT指令执行，硬件自动在内核栈压入：
 * 	int 源SS
 * 		源SP
 * 		EFLAGS
 * 		源CS
 * 		源EIP
 * iret ->
 * /

/* 不带参数的系统调用函数	type_name(void) */
#define _syscall0(type, name) 						\
type name(void)		 								\
{ 													\
	long __res; 									\
	__asm__ volatile ("int $0x80"  					\
		: "=a" (__res)								\
		: "0" (__NR_##name));						\
	if (__res >= 0)	{								\
		return (type) __res; 						\
	}												\
	errno = -__res;									\
	return -1; 										\
}

/* 有1个参数的系统调用函数	type_name(atype a) */
#define _syscall1(type, name, atype, a) 			\
type name(atype a) 									\
{ 													\
	long __res; 									\
	__asm__ volatile ("int $0x80" 					\
		: "=a" (__res) 								\
		: "0" (__NR_##name), "g" ((long)(a))); 		\
	if (__res >= 0) 								\
		return (type) __res; 						\
	errno = -__res; 								\
	return -1; 										\
}

/* 有2个参数的系统调用函数	type_name(atype a,btype b) */
#define _syscall2(type, name, atype, a, btype, b) 	\
type name(atype a, btype b) 						\
{ 													\
	long __res; 									\
	__asm__ volatile ("int $0x80" 					\
		: "=a" (__res)								\
		: "0" (__NR_##name), "g" ((long)(a)), "c" ((long)(b))); 	\
	if (__res >= 0) 								\
		return (type) __res; 						\
	errno = -__res; 								\
	return -1; 										\
}

/* 有3个参数的系统调用函数	type_name(atype a,btype b,ctype c) */
#define _syscall3(type, name, atype, a, btype, b, ctype, c) \
type name(atype a, btype b, ctype c) 				\
{ 													\
	long __res; 									\
	__asm__ volatile ("int $0x80"					\
		: "=a" (__res) 								\
		: "0" (__NR_##name), "g" ((long)(a)), "c" ((long)(b)), "d" ((long)(c))); \
	if (__res >= 0) 								\
		return (type) __res; 						\
	errno = -__res; 								\
	return -1; 										\
}

#endif /* __LIBRARY__ */

extern int errno;
int close(int fildes);
int dup(int fildes);
int execve(const char * filename, char ** argv, char ** envp);
void _exit(int status) __attribute__ ((noreturn));
int fork(void);
int lseek(int fildes, off_t offset, int origin);
int open(const char * filename, int flag, ...);
int pause(void);
int read(int fildes, char * buf, off_t count);
int stat(const char * filename, struct stat * stat_buf);
int fstat(int fildes, struct stat * stat_buf);
int ustat(dev_t dev, struct ustat * ubuf);
pid_t wait(int * wait_stat);
int write(int fildes, const char * buf, off_t count);
pid_t setsid(void);

#endif