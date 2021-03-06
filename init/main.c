#define __LIBRARY__
#include <unistd.h>
#include <errno.h>

// @todo
extern inline _syscall0(int, fork)
extern inline _syscall0(int, pause)
extern inline _syscall1(int, setup, void *, BIOS)
_syscall0(int, sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <time.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <fcntl.h>

#include <linux/fs.h>
#include <c.h>

// #define RAMDISK 2048

static char printbuf[64] = {'Z',};

extern int vsprintf(char * buf, const char * fmt, va_list args);
void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern void con_init(void);
extern long kernel_mktime(struct tm * tm);

#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)		/* 根文件系统所在设备号 */
#define ORIG_SWAP_DEV (*(unsigned short *)0x901FA)		/* 交换文件所在设备号 */

static long memory_end = 0;             /* 机器所具有的物理内存容量 */
static long buffer_memory_end = 0;      /* 高速缓冲区末端地址 */
static long main_memory_start = 0;      /* 主内存开始位置 */

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL ,NULL };

/* 运行登录shell时所使用的命令行和环境参数 */
/* argv[0]中的字符“-”是传递shell程序sh的一个标示位，通过这个标示位，sh程序会作为shell程序执行 */
static char * argv[] = { "-/bin/sh", NULL };
static char * envp[] = { "HOME=/usr/root", NULL, NULL };

#define CMOS_READ(addr) ({		\
	outb_p(0x80 | addr, 0x70);	\
	inb_p(0x71); 				\
})

#define BCD_TO_BIN(val)	((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void) {
    struct tm time;
    do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
    } while (time.tm_sec != CMOS_READ(0));
    BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
    time.tm_mon--;
    startup_time = kernel_mktime(&time);
}
static void printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
    i = vsprintf(printbuf, fmt, args);
	write(1, printbuf, i);
	va_end(args);
}

struct drive_info { char dummy[32]; } drive_info;

void start_kernel(void) {
    ROOT_DEV = ORIG_ROOT_DEV;
    drive_info = DRIVE_INFO;
    memory_end = (1 << 20) + (EXT_MEM_K << 10);
    memory_end &= 0xfffff000;
    if (memory_end > 16 * 1024 * 1024) {
        memory_end = 16 * 1024 * 1024;
    }

    if (memory_end >= 12 * 1024 * 1024) {
        buffer_memory_end = 4 * 1024 * 1024;
    } else if (memory_end >= 6 * 1024 * 1024) {
        buffer_memory_end = 2 * 1024 * 1024;
    } else if (memory_end >= 4 * 1024 * 1024) {
        buffer_memory_end = 3 * 512 * 1024;
    } else {
        buffer_memory_end = 1 * 1024 * 1024;
    }

    main_memory_start = buffer_memory_end;
#ifdef RAMDISK
    main_memory_start += rd_init(main_memory_start, RAMDISK * 1024);
#endif
    
    mem_init(main_memory_start, memory_end);
    trap_init();
    blk_dev_init();
    chr_dev_init();
    tty_init();
    time_init();
    sched_init();
    buffer_init(buffer_memory_end);
    hd_init();
    sti();
    move_to_user_mode();
    if (!fork()) {
        init();
    }
    for (;;)
        __asm__("int $0x80"::"a" (__NR_pause));
}

extern void mytest();
void init(void) {
    char buf[512];
    int pid, i;

    setup((void *) &drive_info);
    (void) open("/dev/tty1", O_RDWR, 0);
    (void) dup(0);
    (void) dup(0);
    
    // printf("this is from printf function\n\n");
    if (!(pid = fork())) {
        close(0);
        if (open("/etc/rc", O_RDONLY, 0)) {
			_exit(1);
		}
        execve("/bin/sh", argv_rc, envp_rc);
		_exit(2);
    }
    if (pid > 0) {
        while (pid != wait(&i)) {};
    }
    while (1) {
        if ((pid = fork()) < 0) {
            printf("Fork failed in init\r\n");
            continue;
        }
        // mytest();
        if (!pid) {
            close(0); close(1); close(2);
            setsid();
            (void) open("/dev/tty1", O_RDWR, 0);
            (void) dup(0);
            (void) dup(0);
            _exit(execve("/bin/sh", argv, envp));
        }
        while (1) {
            if (pid == wait(&i)) {
                break;
            }
        }
        printf("\n\rchild %d died with code %04x\n\r", pid, i);
		sync();
    }
    _exit(0);
}