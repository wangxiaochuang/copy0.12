#include <errno.h>
#include <linux/sched.h>

__attribute__ ((noreturn))
void do_exit(long code) {
	panic("process exit, pid: ", current->pid);
	while(1);
}

void sys_exit(int error_code) {
	do_exit((error_code & 0xff) << 8);
}