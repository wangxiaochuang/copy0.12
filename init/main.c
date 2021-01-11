#include <linux/sched.h>
#include <linux/kernel.h>
#include <time.h>

#include <asm/system.h>
#include <asm/io.h>

#define EXT_MEM_K (*(unsigned short *)0x90002)

extern void mem_init(long start, long end);
extern void con_init(void);
extern long kernel_mktime(struct tm * tm);
extern unsigned long startup_time;

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

void main(void) {
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
    
    mem_init(main_memory_start, memory_end);
    trap_init();
    con_init();
    time_init();
    printk("\nstart time: %u\n", startup_time);
    while(1){
        __asm__ ("hlt"::);
    };
}
