/*
 * Why isn't this a .c file?  Enquiring minds....
 */
/*
 * 为什么这不是一个.c文件？动动脑筋自己想想....
 */

extern int sys_setup();
extern int sys_fork();
extern int sys_pause();
extern int sys_write(unsigned int fd, char * buf, int count);

/* 系统调用处理程序的指针数组表 */
fn_ptr sys_call_table[] = { sys_setup, 0, sys_fork, 0, sys_write, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, sys_pause
};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);
