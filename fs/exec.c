#include <sys/stat.h>
#include <a.out.h>

#include <linux/sched.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

int do_execve(unsigned long * eip, long tmp, char * filename,
	char ** argv, char ** envp) {

    if ((0xffff & eip[1]) != 0x000f) {
		panic("execve called from supervisor mode");
	}
    panic("i am here.........");
}