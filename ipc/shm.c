#include <linux/errno.h>
#include <asm/segment.h>
#include <linux/sched.h>
#include <linux/ipc.h> 
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/malloc.h>

void shm_init (void) {

}

int shm_fork (struct task_struct *p1, struct task_struct *p2) {
    if (!p1->shm)
        return 0;
    return 0;
}

int shm_swap (int prio) {
    return 0;
}