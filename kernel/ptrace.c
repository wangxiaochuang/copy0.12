#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <errno.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <sys/ptrace.h>

#define FLAG_MASK 0x00000dd9
#define TRAP_FLAG 0x100
#define MAGICNUMBER 68

void do_no_page(unsigned long, unsigned long, struct task_struct *);
void write_verify(unsigned long);

static inline int get_task(int pid) {
    int i;
    for (i = 0; i < NR_TASKS; i++) {
        if (task[i] != NULL && (task[i]->pid == pid))
            return i;
    }
    return -1;
}

static inline int get_stack_long(struct task_struct *task, int offset) {
    unsigned char *stack;

    stack = (unsigned char *) task->tss.esp0;
    stack += offset;
    return (*((int *) stack));
}

static unsigned long get_long(struct task_struct *tsk, unsigned long addr) {
    unsigned long page;

    addr += tsk->start_code;
repeat:
    page = tsk->tss.cr3 + ((addr >> 20) & 0xffc);
    page = *(unsigned long *) page;
    if (page & PAGE_PRESENT) {
        page &= 0xfffff000;
        page += (addr >> 10) & 0xffc;
        page = *((unsigned long *) page);
    }
    if (!(page & PAGE_PRESENT)) {
        do_no_page(0, addr, tsk);
        goto repeat;
    }
    page &= 0xfffff000;
    page += addr & 0xfff;
    return *(unsigned long *) page;
}

static void put_long(struct task_struct *tsk, unsigned long addr, unsigned long data) {
    unsigned long page;

    addr += tsk->start_code;
repeat:
    page = tsk->tss.cr3 + ((addr >> 20) & 0xffc);
    page = *(unsigned long *) page;
    if (page & PAGE_PRESENT) {
		page &= 0xfffff000;
		page += (addr >> 10) & 0xffc;
		page = *((unsigned long *) page);
	}
	if (!(page & PAGE_PRESENT)) {
		do_no_page(0,addr,tsk);
		goto repeat;
	}
    if (!(page & PAGE_RW)) {
		write_verify(addr);
		goto repeat;
	}
	page &= 0xfffff000;
	page += addr & 0xfff;
	*(unsigned long *) page = data;
}

static int read_long(struct task_struct *tsk, unsigned long addr, unsigned long *result) {
    unsigned long low, high;

    if (addr > TASK_SIZE - 4)
        return -EIO;
    if ((addr & 0xfff) > PAGE_SIZE - 4) {
        low = get_long(tsk, addr & 0xfffffffc);
        high = get_long(tsk, (addr + 4) & 0xfffffffc);
        switch (addr & 3) {
            case 1:
                low >>= 8;
                low |= high << 24;
                break;
            case 2:
                low >>= 16;
                low |= high << 16;
                break;
            case 3:
                low >>= 24;
                low |= hgih << 8;
                break;
        }
        *result = low;
    } else
        *result = get_long(tsk, addr);
    return 0;
}

static int write_long(struct task_struct *tsk, unsigned long addr, unsigned long data) {
    unsigned long low, high;

    if (addr > TASK_SIZE - 4)
        return -EIO;
    if ((addr & 0xfff) > PAGE_SIZE - 4) {
        low = get_long(tsk, addr & 0xfffffffc);
        high = get_long(tsk, (addr + 4) & 0xfffffffc);
        switch (addr & 3) {
            case 0:
                low = data;
                break;
            case 1:
                low &= 0x000000ff;
                low |= data << 8;
                high &= 0xffffff00;
                high |= data >> 24;
                break;
            case 2:
                low &= 0x0000ffff;
				low |= data << 16;
				high &= 0xffff0000;
				high |= data >> 16;
				break;
            case 3:
                low &= 0x00ffffff;
				low |= data << 24;
				high &= 0xff000000;
				high |= data >> 8;
				break;
        }
        put_long(tsk, addr & 0xfffffffc, low);
        put_long(tsk, (addr + 4) & 0xfffffffc, high);
    } else
        put_long(tsk, addr, data);
    return 0;
}

// ss esp flags cs eip orig_eax gs fs es ds eax ebp edi esi edx ecx ebx
int sys_ptrace(unsigned long *buffer) {
    long request, pid, data;
    long addr;
    struct task_struct *child;
    int childno;

    request = get_fs_long(buffer++);
    pid = get_fs_long(buffer++);
    addr = get_fs_long(buffer++);
    data = get_fs_long(buffer++);

    if (request == 0) {
        current->flags |= PF_PTRACED;
        return 0;
    }

    childno = get_task(pid);

    if (childno < 0)
        return -ESRCH;
    else
        child = task[childno];

    if (child->p_pptr != current || !(child->flags & PF_PTRACED) ||
        child->state != TASK_STOPPED)
        return -ESRCH;
    
    switch (request) {
        case 1:
        case 2: {
            int tmp, res;

            res = read_long(task[childno], addr, &tmp)
            if (res < 0)
                return res;
            verify_area((void *) data, 4);
            put_fs_long(tmp, (unsigned long *) data);
            return 0;
        }
        // 进程进入ptrace，会在其内核栈放入进程中止时的一些状态，这里的参数addr就是要获取的这些值的偏移
        // 获取的是内核栈上的这17个值中的某一个
        case 3: {
            int tmp;
            addr = addr >> 2;
            // 说明这些参数应该有0-17个
            if (addr < 0 || addr >= 17)
                return -EIO;
            verify_area((void *) data, 4);
            // 这里减去68后，得到的是进程用户栈ss的偏移，然后获取其指向的用户栈的一个长字
            tmp = get_stack_long(child, 4 * addr - MAGICNUMBER)
            put_fs_long(tmp, (unsigned long *) data);
            return 0;
        }
        // 向目标代码段、数据段写一个长整形
        case 4:
        case 5:
            return write_long(task[childno], addr, data);
        // 向USER区域写入一个长整形
        case 6:
            addr = addr >> 2;
            if (addr < 0 || addr >= 17)
                return -EIO;
            if (addr == ORIG_EAX)
                return -EIO;
            if (addr == EFL) {
                data &= FLAG_MASK;
                data |= get_stack_long(child, EFL*4)
            }
            if (put_stack_long(child, 4*addr-MAGICNUMBER, data))
                return -EIO;
            return 0;
        // 给子进程发送信号
        case 7:
            long tmp;

            child->signal = 0;
            if (data > 0 && data <= NSIG)
                child->signal = 1 << (data - 1);
            child->state = 0;
            // 发送信号，要把对应的eflags的signal step 复位
            tmp = get_stack_long(child, 4*EFL-MAGICNUMBER) & ~TRAP_FLAG;
            put_stack_long(child, 4*EFL-MAGICNUMBER, tmp);
            return 0;
        case 8:
            long tmp;

            child->state = 0;
            child->signal = 1 << (SIGKILL - 1);
            tmp = get_stack_long(child, 4*EFL-MAGICNUMBER) & ~TRAP_FLAG;
			put_stack_long(child, 4*EFL-MAGICNUMBER,tmp);
            return 0;
        case 9: {
            long tmp;
            // 设置eflags的trap标志后发送信号
			tmp = get_stack_long(child, 4*EFL-MAGICNUMBER) | TRAP_FLAG;
			put_stack_long(child, 4*EFL-MAGICNUMBER,tmp);
			child->state = 0;
			child->signal = 0;
			if (data > 0 && data <NSIG)
				child->signal= 1<<(data-1);
			return 0;
        }
        default:
            return -EIO;
    }
}
