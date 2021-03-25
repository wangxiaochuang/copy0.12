#include <asm/segment.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <linux/string.h>

static int dupfd(unsigned int fd, unsigned int arg) {
	if (fd >= NR_OPEN || !current->filp[fd])
		return -EBADF;
	if (arg >= NR_OPEN)
		return -EINVAL;
	while (arg < NR_OPEN)
		if (current->filp[arg])
			arg++;
		else
			break;
	if (arg >= NR_OPEN)
		return -EMFILE;
	FD_CLR(arg, &current->close_on_exec);
	(current->filp[arg] = current->filp[fd])->f_count++;
	return arg;
}

asmlinkage int sys_dup(unsigned int fildes) {
	return dupfd(fildes, 0);
}