void verify_area(void * addr, int count);
void panic(const char * fmt, ...);
void do_exit(long error_code) __attribute__ ((noreturn));
void console_print(const char * str);
int tty_write(unsigned ch,char * buf,int count);
int printk(const char * fmt, ...);
int dumpmem(const void *address, int len);
int printkl(const char *str, int len);
extern void hd_times_out(void);
extern void sysbeepstop(void);          /* 停止蜂鸣 */
extern void blank_screen(void);         /* 黑屏处理 */
extern void unblank_screen(void);       /* 恢复被黑屏的屏幕 */

extern int beepcount;       /* 蜂鸣时间滴答计数 */
extern int hd_timeout;      /* 硬盘超时滴答值 */
extern int blankinterval;   /* 设定的屏幕黑屏间隔时间 */
extern int blankcount;      /* 黑屏时间计数 */

#define suser() (current->euid == 0)