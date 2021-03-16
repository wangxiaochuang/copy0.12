#include <stdarg.h>

#include <asm/segment.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#define LOG_BUF_LEN	4096

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);
extern void console_print(const char *);

#define DEFAULT_MESSAGE_LOGLEVEL 7 /* KERN_DEBUG */
#define DEFAULT_CONSOLE_LOGLEVEL 7 /* anything more serious than KERN_DEBUG */

unsigned long log_size = 0;
struct wait_queue * log_wait = NULL;
int console_loglevel = DEFAULT_CONSOLE_LOGLEVEL;

static void (*console_print_proc)(const char *) = 0;
static char log_buf[LOG_BUF_LEN];
static unsigned long log_start = 0;
static unsigned long logged_chars = 0;

asmlinkage int printk(const char *fmt, ...) {
	va_list args;
	int i;
	char *msg, *p, *buf_end;
	static char msg_level = -1;
	long flags;

	save_flags(flags);
	cli();
	va_start(args, fmt);
	i = vsprintf(buf + 3, fmt, args);
	buf_end = buf + 3 + i;
	va_end(args);
	for (p = buf + 3; p < buf_end; p++) {
		msg = p;
		if (msg_level < 0) {
			if (p[0] != '<' || p[1] < '0' || p[1] > '7' || p[2] != '>') {
				p -= 3;
				p[0] = '<';
				p[1] = DEFAULT_MESSAGE_LOGLEVEL - 1 + '0';
				p[2] = '>';
			} else {
				msg += 3;
			}
			msg_level = p[1] - '0';
		}
		for (; p < buf_end; p++) {
			log_buf[(log_start + log_size) & (LOG_BUF_LEN - 1)] = *p;
			if (log_size < LOG_BUF_LEN)
				log_size++;
			else
				log_start++;
			logged_chars++;
			if (*p == '\n')
				break;
		}
		if (msg_level < console_loglevel && console_print_proc) {
			char tmp = p[1];
			p[1] = '\0';
			(*console_print_proc)(msg);
			p[1] = tmp;
		}
		if (*p == '\n')
			msg_level = -1;
	}
	restore_flags(flags);
	wake_up_interruptible(&log_wait);
	return i;
}

void register_console(void (*proc)(const char *)) {
    int	i,j;
	int	p = log_start;
	char	buf[16];
	char	msg_level = -1;
	char	*q;

    console_print_proc = proc;

    for (i = 0, j = 0; i < log_size; i++) {
        buf[j++] = log_buf[p];
        p++; p &= LOG_BUF_LEN-1;
        if (buf[j-1] != '\n' && i < log_size - 1 && j < sizeof(buf)-1)
			continue;
        buf[j] = 0;
        q = buf;
        if (msg_level < 0) {
			msg_level = buf[1] - '0';
			q = buf + 3;
		}
		if (msg_level < console_loglevel)
			(*proc)(q);
		if (buf[j-1] == '\n')
			msg_level = -1;
		j = 0;
    }
}