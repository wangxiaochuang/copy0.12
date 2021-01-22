#include <errno.h>
#include <linux/sched.h>

int sys_kill(int pid,int sig) {
	// @todo
	return 0;
}

int kill_pg(int pgrp, int sig, int priv) {
	// @todo
	return 0;
}

int is_orphaned_pgrp(int pgrp) {
	// @todo
	return 0;
}

__attribute__ ((noreturn))
void do_exit(long code) {
	panic("process exit, pid: ", current->pid);
	while(1);
}

void sys_exit(int error_code) {
	do_exit((error_code & 0xff) << 8);
}