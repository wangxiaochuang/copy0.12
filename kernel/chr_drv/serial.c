#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

#define WAKEUP_CHARS (TTY_BUF_SIZE/4)

extern void rs1_interrupt(void);
extern void rs2_interrupt(void);

static void com1_timer(void) {
	TTY_READ_FLUSH(tty_table+64);
}

static void com2_timer(void) {
	TTY_READ_FLUSH(tty_table+65);
}

static void com3_timer(void) {
	TTY_READ_FLUSH(tty_table+66);
}

static void com4_timer(void) {
	TTY_READ_FLUSH(tty_table+67);
}

static inline void do_rs_write(unsigned int port) {}

static void com1_timeout(void) {
	do_rs_write(0);
}

static void com2_timeout(void) {
	do_rs_write(1);
}

static void com3_timeout(void) {
	do_rs_write(2);
}

static void com4_timeout(void) {
	do_rs_write(3);
}

static void init(int port) {
	outb_p(0x80, port+3);	/* set DLAB of line control reg */
	outb_p(0x30, port);	/* LS of divisor (48 -> 2400 bps */
	outb_p(0x00, port+1);	/* MS of divisor */
	outb_p(0x03, port+3);	/* reset DLAB */
	outb_p(0x0b, port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d, port+1);	/* enable all intrs but writes */
	(void)inb(port);	/* read data port to reset things (?) */
}

void serial_open(unsigned int line) {
    unsigned short port;
    unsigned short port2;

    if (line > 3)
        return;
    port = tty_table[64+line].read_q->data;
    if (!port)
        return;
    port = tty_table[64+(line ^ 2)].read_q->data;
    cli();
    if (port2)
        outb_p(0x00, port2 + 4);
    outb_p(0x03,port+3);	/* reset DLAB */
	outb_p(0x0f,port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d,port+1);	/* enable all intrs but writes */
	inb_p(port+5);
	inb_p(port+0);
	inb(port+6);
	inb(port+2);
	sti();
}

void rs_init(void) {
/* SERx_TIMER timers are used for receiving: timeout is always 0 (immediate) */
	timer_table[SER1_TIMER].fn = com1_timer;
	timer_table[SER1_TIMER].expires = 0;
	timer_table[SER2_TIMER].fn = com2_timer;
	timer_table[SER2_TIMER].expires = 0;
	timer_table[SER3_TIMER].fn = com3_timer;
	timer_table[SER3_TIMER].expires = 0;
	timer_table[SER4_TIMER].fn = com4_timer;
	timer_table[SER4_TIMER].expires = 0;
/* SERx_TIMEOUT timers are used for writing: prevent serial lockups */
	timer_table[SER1_TIMEOUT].fn = com1_timeout;
	timer_table[SER1_TIMEOUT].expires = 0;
	timer_table[SER2_TIMEOUT].fn = com2_timeout;
	timer_table[SER2_TIMEOUT].expires = 0;
	timer_table[SER3_TIMEOUT].fn = com3_timeout;
	timer_table[SER3_TIMEOUT].expires = 0;
	timer_table[SER4_TIMEOUT].fn = com4_timeout;
	timer_table[SER4_TIMEOUT].expires = 0;
    set_intr_gate(0x24, rs1_interrupt);
    set_intr_gate(0x23, rs2_interrupt);
    init(tty_table[64].read_q->data);
    init(tty_table[65].read_q->data);
    init(tty_table[66].read_q->data);
    init(tty_table[67].read_q->data);
    outb(inb_p(0x21)&0xE7, 0x21);
}
void rs_write(struct tty_struct * tty) {
    cli();
    if (!EMPTY(tty->write_q)) {
        outb(inb_p(tty->write_q->data + 1) | 0x02, tty->write_q->data + 1);
    }
    sti();
}