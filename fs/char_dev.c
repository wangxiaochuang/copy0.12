#include <errno.h>
#include <sys/types.h>

#include <linux/sched.h>

typedef int (*crw_ptr)(int rw, unsigned minor, char * buf, int count, off_t * pos);

static int rw_ttyx(int rw, unsigned minor, char * buf, int count, off_t * pos) {
    return 0;
}

static int rw_tty(int rw,unsigned minor,char * buf,int count, off_t * pos) {
    return 0;
}

static int rw_memory(int rw, unsigned minor, char * buf, int count, off_t * pos) {
    return 0;
}
static crw_ptr crw_table[]={
	NULL,		/* nodev */
	rw_memory,	/* /dev/mem etc */
	NULL,		/* /dev/fd */
	NULL,		/* /dev/hd */
	rw_ttyx,	/* /dev/ttyx */
	rw_tty,		/* /dev/tty */
	NULL,		/* /dev/lp */
	NULL
};		/* unnamed pipes */

int rw_char(int rw, int dev, char * buf, int count, off_t * pos) {
    panic("i am here in rw_char.........\n");
}