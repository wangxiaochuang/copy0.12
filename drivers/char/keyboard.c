#define KEYBOARD_IRQ 1

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/config.h>
#include <linux/signal.h>
#include <linux/string.h>

#include <asm/bitops.h>

#include "kbd_kern.h"

#define SIZE(x) (sizeof(x)/sizeof((x)[0]))

#ifndef KBD_DEFMODE
#define KBD_DEFMODE ((1 << VC_REPEAT) | (1 << VC_META))
#endif

#ifndef KBD_DEFLEDS
/*
 * Some laptops take the 789uiojklm,. keys as number pad when NumLock
 * is on. This seems a good reason to start with NumLock off.
 */
#define KBD_DEFLEDS 0
#endif

#ifndef KBD_DEFLOCK
#define KBD_DEFLOCK 0
#endif

/* shift state counters.. */
static unsigned char k_down[NR_SHIFT] = {0, };
/* keyboard key bitmap */
static unsigned long key_down[8] = { 0, };

static int shift_state = 0;
static char rep = 0;
struct kbd_struct kbd_table[NR_CONSOLES];
static struct kbd_struct * kbd = kbd_table;
static struct tty_struct * tty = NULL;

static void keyboard_interrupt(int int_pt_regs) {
    return;
}

void compute_shiftstate(void) {
    int i, j, k, sym, val;

    shift_state = 0;
	for(i=0; i < SIZE(k_down); i++)
        k_down[i] = 0;

	for(i=0; i < SIZE(key_down); i++)
        if(key_down[i]) {	/* skip this word if not a single bit on */
            k = (i<<5);
            for(j=0; j<32; j++,k++)
                if(test_bit(k, key_down)) {
                    sym = key_map[0][k];
                    if(KTYP(sym) == KT_SHIFT) {
                        val = KVAL(sym);
                        k_down[val]++;
                        shift_state |= (1<<val);
                    }
                }
        }
}

static void kbd_bh(void * unused) {
    return;
}

unsigned long kbd_init(unsigned long kmem_start) {
    int i;
    struct kbd_struct *kbd;

    kbd = kbd_table + 0;
    for (i = 0; i < NR_CONSOLES; i++, kbd++) {
        kbd->ledstate = KBD_DEFLEDS;
        kbd->default_ledstate = KBD_DEFLEDS;
        kbd->lockstate = KBD_DEFLOCK;
        kbd->modeflags = KBD_DEFMODE;
    }
    bh_base[KEYBOARD_BH].routine = kbd_bh;
    request_irq(KEYBOARD_IRQ, keyboard_interrupt);
    mark_bh(KEYBOARD_BH);
    return kmem_start;
}