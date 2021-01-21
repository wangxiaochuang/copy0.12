#include <linux/tty.h>

struct tty_struct tty_table[256];

int fg_console = 0;

void chr_dev_init(void) {}
