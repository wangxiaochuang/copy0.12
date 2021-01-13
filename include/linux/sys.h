/*
 * Why isn't this a .c file?  Enquiring minds....
 */
/*
 * 为什么这不是一个.c文件？动动脑筋自己想想....
 */

extern int sys_fork();

/* 系统调用处理程序的指针数组表 */
fn_ptr sys_call_table[] = { 0, 0, sys_fork};

/* So we don't have to do any more manual updating.... */
int NR_syscalls = sizeof(sys_call_table)/sizeof(fn_ptr);
