#define __LIBRARY__
#include <unistd.h>
#include <errno.h>
static inline _syscall0(int, fork)
static inline _syscall0(int, pause)
static inline _syscall1(int, setup, void *, BIOS)

#include <linux/sched.h>
#include <linux/kernel.h>
#include <time.h>

#include <asm/system.h>
#include <asm/io.h>
#include <stddef.h>
#include <stdarg.h>

#include <linux/fs.h>

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
    unsigned long tmp = kernel_mktime(&time);
    startup_time = tmp;
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

void main(void) {
    ROOT_DEV = ORIG_ROOT_DEV;
    SWAP_DEV = ORIG_SWAP_DEV;
    drive_info = DRIVE_INFO;
    memory_end = (1 << 20) + (EXT_MEM_K << 10);
    memory_end &= 0xfffff000;
    if (memory_end > 16 * 1024 * 1024) {
        memory_end = 16 * 1024 * 1024;
    }

    if (memory_end > 12 * 1024 * 1024) {
        buffer_memory_end = 4 * 1024 * 1024;
    } else if (memory_end > 6 * 1024 * 1024) {
        buffer_memory_end = 2 * 1024 * 1024;
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
    con_init();
    time_init();
    sched_init();
    buffer_init(buffer_memory_end);
    hd_init();
    printk("\nstart time: %u\n", startup_time);
    sti();

    move_to_user_mode();
    if (!fork()) {
        init();
    }
    while(1){
        __asm__("int $0x80"::"a" (__NR_pause));
    };
}

void init(void) {
    setup((void *) &drive_info);
    for (;;) ;
}