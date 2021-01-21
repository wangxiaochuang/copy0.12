/*
 * Why isn't this a .c file?  Enquiring minds....
 */
/*
 * 为什么这不是一个.c文件？动动脑筋自己想想....
 */

extern int sys_setup();
extern int sys_exit();
extern int sys_fork();
extern int sys_write(unsigned int fd, char * buf, int count);
extern int sys_open();
extern int sys_close();
extern int sys_execve();
extern int sys_dup();
extern int sys_pause();

/* 系统调用处理程序的指针数组表 */
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, 0, sys_write, sys_open, sys_close, 0, 0, 0,
    0, sys_execve, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, sys_pause,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, sys_dup, 0, 0, 0, 0, 0, 0, 0, 0
};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);
