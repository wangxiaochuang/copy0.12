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
extern int sys_execve();
extern int sys_stat();
extern int sys_lseek();
extern int sys_kill();
extern int sys_dup();
extern int sys_fstat();
extern int sys_signal();
extern int sys_pause();
extern int sys_uname();
extern int sys_ustat();
extern int sys_sigaction();
extern int sys_sgetmask();
extern int sys_ssetmask();
extern int sys_sigpending();
extern int sys_sigsuspend();
extern int sys_sethostname();
extern int sys_lstat();
extern int sys_unimpl();

/* 系统调用处理程序的指针数组表 */
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read, sys_write, sys_open, sys_close, sys_unimpl, sys_unimpl, sys_unimpl,
    sys_unimpl, sys_execve, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_stat, sys_lseek,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_fstat, sys_pause,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_kill, sys_unimpl, sys_unimpl,
    sys_unimpl, sys_dup, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_signal, sys_unimpl,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_uname,
    sys_unimpl, sys_unimpl, sys_ustat, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_sigaction, sys_sgetmask, sys_ssetmask,
    sys_unimpl, sys_unimpl, sys_sigsuspend, sys_sigpending, sys_sethostname, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl,
    sys_unimpl, sys_unimpl, sys_unimpl, sys_unimpl, sys_lstat, sys_unimpl, sys_unimpl
};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);
