void panic(const char * fmt, ...);
void do_exit(long error_code) __attribute__ ((noreturn));
void console_print(const char * str);
int printk(const char * fmt, ...);
int dumpmem(const void *address, int len);
extern void hd_times_out(void);

extern int hd_timeout;      /* 硬盘超时滴答值 */

#define suser() (current->euid == 0)