#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

static inline void pty_copy(struct tty_struct * from, struct tty_struct * to) {
    char c;

    while (!from->stopped && !EMPTY(from->write_q)) {
        // 目标的读队列满了，就调用其转换函数消费读队列
        if (FULL(to->read_q)) {
            if (FULL(to->secondary))
                break;
            copy_to_cooked(to);
            continue;
        }
        GETCH(from->write_q, c);
        PUTCH(c, to->read_q);
        if (current->signal && ~current->blocked)
            break;
    }
    copy_to_cooked(to);
    wake_up(&from->write_q->proc_list);
}

void mpty_write(struct tty_struct *tty) {
    int nr = tty - tty_table;

    if ((nr >> 6) != 2)
        printk("bad mpty\n\r");
    else
        pty_copy(tty, tty + 64);
}

void spty_write(struct tty_struct * tty) {
    int nr = tty - tty_table;
    if ((nr >> 6) != 3)
        printk("bad spty\n\r");
    else
        pty_copy(tty, tty - 64);
}