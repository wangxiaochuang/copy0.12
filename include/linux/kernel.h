void panic(const char * fmt, ...);
void console_print(const char * str);
int printk(const char * fmt, ...);
extern void hd_times_out(void);

extern int hd_timeout;      /* 硬盘超时滴答值 */