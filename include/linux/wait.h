#ifndef _LINUX_WAIT_H
#define _LINUX_WAIT_H

struct wait_queue {
	struct task_struct * task;
	struct wait_queue * next;
};

struct semaphore {
	int count;
	struct wait_queue *wait;
};

typedef struct select_table_struct {
	int nr;
	struct select_table_entry * entry;
} select_table;

#endif