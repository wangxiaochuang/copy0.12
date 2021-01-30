/*
 * Why isn't this a .c file?  Enquiring minds....
 */
/*
 * 为什么这不是一个.c文件？动动脑筋自己想想....
 */

extern int sys_setup();
extern int sys_exit();
extern int sys_fork();
extern int sys_read();
extern int sys_write();
extern int sys_open();
extern int sys_close();
extern int sys_waitpid();
extern int sys_creat();
extern int sys_execve();
extern int sys_chdir();
extern int sys_time();
extern int sys_stat();
extern int sys_lseek();
extern int sys_getpid();
extern int sys_sync();
extern int sys_kill();
extern int sys_dup();
extern int sys_pipe();
extern int sys_times();
extern int sys_brk();
extern int sys_setuid();
extern int sys_getuid();
extern int sys_stime();
extern int sys_alarm();
extern int sys_fstat();
extern int sys_getgid();
extern int sys_signal();
extern int sys_geteuid();
extern int sys_getegid();
extern int sys_pause();
extern int sys_ioctl();
extern int sys_fcntl();
extern int sys_umask();
extern int sys_ulimit();
extern int sys_uname();
extern int sys_ustat();
extern int sys_dup2();
extern int sys_getppid();
extern int sys_setsid();
extern int sys_sigaction();
extern int sys_sgetmask();
extern int sys_ssetmask();
extern int sys_sigpending();
extern int sys_sigsuspend();
extern int sys_sethostname();
extern int sys_lstat();
extern int sys_unimpl();

/* 系统调用处理程序的指针数组表 */
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read, sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_unimpl,
    sys_unimpl, sys_execve, sys_chdir, sys_time, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_stat, sys_lseek,
    sys_getpid, sys_unimpl, sys_unimpl, sys_setuid, sys_getuid, sys_stime, sys_unimpl, sys_alarm, sys_fstat, sys_pause,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_sync, sys_kill, sys_unimpl, sys_unimpl,
    sys_unimpl, sys_dup, sys_pipe, sys_times, sys_unimpl, sys_brk, sys_unimpl, sys_getgid, sys_signal, sys_geteuid,
    sys_getegid, sys_unimpl, sys_unimpl, sys_unimpl, sys_ioctl, sys_fcntl, sys_unimpl, sys_unimpl, sys_ulimit, sys_uname,
    sys_umask, sys_unimpl, sys_ustat, sys_dup2, sys_getppid, sys_unimpl, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
    sys_unimpl, sys_unimpl, sys_sigsuspend, sys_sigpending, sys_sethostname, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_lstat, sys_unimpl, sys_unimpl
};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);
