#include <linux/config.h>
#include <linux/errno.h>
#include <asm/segment.h>
#include <linux/sched.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>

extern void sem_init (void), msg_init (void), shm_init (void);

void ipc_init (void) {
    sem_init();
    msg_init();
    shm_init();
    return; 
}