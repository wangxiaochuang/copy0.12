#include <stdarg.h>

#include <asm/system.h>
#include <asm/io.h>

#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/head.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/utsname.h>
#include <linux/ioport.h>

extern char edata, end;
extern char *linux_banner;
asmlinkage void lcall7(void);
struct desc_struct default_ldt;

#define __NR__exit __NR_exit
static inline _syscall0(int, idle)
static inline _syscall0(int, fork)
static inline _syscall1(int, setup, void *, BIOS)
static inline _syscall3(int,write,int,fd,const char *,buf,off_t,count)
static inline _syscall1(int, dup, int, fd)
static inline _syscall3(int, open, const char *, file, int, flag, int, mode)

static char printbuf[1024];

extern int console_loglevel;

extern char empty_zero_page[PAGE_SIZE];
extern int vsprintf(char *,const char *,va_list);
extern void init(void);
extern void init_IRQ(void);
extern long kmalloc_init (long,long);
extern long blk_dev_init(long,long);
extern long chr_dev_init(long,long);
extern void floppy_init(void);
extern void sock_init(void);
unsigned long net_dev_init(unsigned long, unsigned long);
extern unsigned long simple_strtoul(const char *,char **,unsigned int);

#ifdef CONFIG_SYSVIPC
extern void ipc_init(void);
#endif

#define PARAM	empty_zero_page
#define EXT_MEM_K (*(unsigned short *) (PARAM+2))
#define DRIVE_INFO (*(struct drive_info_struct *) (PARAM+0x80))
#define SCREEN_INFO (*(struct screen_info *) (PARAM+0))
#define MOUNT_ROOT_RDONLY (*(unsigned short *) (PARAM+0x1F2))
#define RAMDISK_SIZE (*(unsigned short *) (PARAM+0x1F8))
#define ORIG_ROOT_DEV (*(unsigned short *) (PARAM+0x1FC))
#define AUX_DEVICE_INFO (*(unsigned char *) (PARAM+0x1FF))

#define MAX_INIT_ARGS 8
#define MAX_INIT_ENVS 8
#define COMMAND_LINE ((char *) (PARAM+2048))

extern void time_init(void);

static unsigned long memory_start = 0;
static unsigned long memory_end = 0;
static unsigned long low_memory_start = 0;

static char term[21];
int rows, cols;

static char * argv_init[MAX_INIT_ARGS+2] = { "init", NULL, };
static char * envp_init[MAX_INIT_ENVS+2] = { "HOME=/", term, NULL, };

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", term, NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", term, NULL };

struct drive_info_struct { char dummy[32]; } drive_info;
struct screen_info screen_info;

unsigned char aux_device_present;
int ramdisk_size;
int root_mountflags = 0;

static char fpu_error = 0;

static char command_line[80] = { 0, };

char *get_options(char *str, int *ints) {
	char *cur = str;
	int i=1;

	while (cur && isdigit(*cur) && i <= 10) {
		ints[i++] = simple_strtoul(cur, NULL, 0);
		if ((cur = strchr(cur,',')) != NULL)
			cur++;
	}
	ints[0] = i - 1;
	return(cur);
}

struct {
	char *str;
	void (*setup_func)(char *, int *);
} bootsetups[] = {
	{ "reserve=", reserve_setup },
    /*
#ifdef CONFIG_INET
	{ "ether=", eth_setup },
#endif
#ifdef CONFIG_BLK_DEV_HD
	{ "hd=", hd_setup },
#endif
#ifdef CONFIG_BUSMOUSE
	{ "bmouse=", bmouse_setup },
#endif
#ifdef CONFIG_SCSI_SEAGATE
	{ "st0x=", st0x_setup },
	{ "tmc8xx=", tmc8xx_setup },
#endif
#ifdef CONFIG_SCSI_T128
	{ "t128=", t128_setup },
#endif
#ifdef CONFIG_SCSI_GENERIC_NCR5380
	{ "ncr5380=", generic_NCR5380_setup },
#endif
#ifdef CONFIG_SCSI_AHA152X
        { "aha152x=", aha152x_setup},
#endif
#ifdef CONFIG_BLK_DEV_XD
	{ "xd=", xd_setup },
#endif
#ifdef CONFIG_MCD
	{ "mcd=", mcd_setup },
#endif
#ifdef CONFIG_SOUND
	{ "sound=", sound_setup },
#endif
#ifdef CONFIG_SBPCD
	{ "sbpcd=", sbpcd_setup },
#endif
*/
	{ 0, 0 }
};

int checksetup(char *line) {
    int i = 0;
    int ints[11];

    while (bootsetups[i].str) {
        int n = strlen(bootsetups[i].str);
        if (!strncmp(line, bootsetups[i].str, n)) {
            bootsetups[i].setup_func(get_options(line + n, ints), ints);
            return 0;
        }
        i++;
    }
    return 0;
}

unsigned long loops_per_sec = 1;

static void calibrate_delay(void) {
    int ticks;
    printk("Calibrating delay loop.. ");
    while (loops_per_sec <<= 1) {
        ticks = jiffies;
        __delay(loops_per_sec);
        ticks = jiffies - ticks;
        if (ticks >= HZ) {
            __asm__("mull %1 ; divl %2"
				:"=a" (loops_per_sec)
				:"d" (HZ),
				 "r" (ticks),
				 "0" (loops_per_sec));
			printk("ok - %lu.%02lu BogoMips\n",
				loops_per_sec/500000,
				(loops_per_sec/5000) % 100);
			return;
        }
    }
    printk("failed\n");
}

static void parse_options(char *line) {
    char *next;
	char *devnames[] = { "hda", "hdb", "sda", "sdb", "sdc", "sdd", "sde", "fd", "xda", "xdb", NULL };
	int devnums[]    = { 0x300, 0x340, 0x800, 0x810, 0x820, 0x830, 0x840, 0x200, 0xC00, 0xC40, 0};
	int args, envs;

    if (!*line)
        return;
    args = 0;
    envs = 1;
    next = line;
    while ((line = next) != NULL) {
        if ((next = strchr(line, ' ')) != NULL)
            *next++ = 0;
        if (!strncmp(line, "root=", 5)) {
            int n;
            line += 5;
            if (strncmp(line, "/dev/", 5)) {
                ROOT_DEV = simple_strtoul(line, NULL, 16);
                continue;
            }
            // 说明就是 /dev/
            line += 5;
            for (n = 0; devnames[n]; n++) {
                int len = strlen(devnames[n]);
                if (!strncmp(line, devnames[n], len)) {
                    ROOT_DEV = devnums[n] + simple_strtoul(line + len, NULL, 16);
                    break;
                }
            }
        } else if (!strcmp(line, "ro"))
            root_mountflags |= MS_RDONLY;
        else if (!strcmp(line, "rw"))
            root_mountflags &= ~MS_RDONLY;
        else if (!strcmp(line, "debug"))
            console_loglevel = 10;
        else if (!strcmp(line, "no387")) {
            hard_math = 0;
            __asm__("movl %%cr0,%%eax\n\t"
				"orl $0xE,%%eax\n\t"
				"movl %%eax,%%cr0\n\t"::);   
        } else {
            checksetup(line);
        }
        // 检查是不是环境变量
        if (strchr(line, '=')) {
            if (envs >= MAX_INIT_ENVS)
                break;
            envp_init[++envs] = line;
        } else {
            if (args >= MAX_INIT_ARGS)
                break;
            argv_init[++args] = line;
        }
    }
    argv_init[args+1] = NULL;
    envp_init[envs+1] = NULL;
}

static void copy_options(char * to, char * from) {
    char c = ' ';
    do {
        if (c == ' ' && !memcmp("mem=", from, 4))
            memory_end = simple_strtoul(from + 4, &from, 0);
        c = *(to++) = *(from++);
    } while (c);
}

static void copro_timeout(void) {
    fpu_error = 1;
    timer_table[COPRO_TIMER].expires = jiffies + 100;
    timer_active |= 1 << COPRO_TIMER;
    printk("387 failed: trying to reset\n");
	send_sig(SIGFPE, last_task_used_math, 1);
	outb_p(0,0xf1);
	outb_p(0,0xf0);
}

asmlinkage void start_kernel(void) {
    set_call_gate(&default_ldt, lcall7);
    ROOT_DEV = ORIG_ROOT_DEV;
    drive_info = DRIVE_INFO;
    screen_info = SCREEN_INFO;
    aux_device_present = AUX_DEVICE_INFO;
    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= PAGE_MASK;
    ramdisk_size = RAMDISK_SIZE;
    copy_options(command_line, COMMAND_LINE);
#ifdef CONFIG_MAX_16M
    if (memory_end > 16*1024*1024)
        memory_end = 16*1024*1024;
#endif
    if (MOUNT_ROOT_RDONLY)
        root_mountflags |= MS_RDONLY;
    if ((unsigned long)&end >= (1024*1024)) {
        memory_start = (unsigned long) &end;
        low_memory_start = PAGE_SIZE;
    } else {
        memory_start = 1024*1024;
        low_memory_start = (unsigned long) &end;
    }

    low_memory_start = PAGE_ALIGN(low_memory_start);
    memory_start = paging_init(memory_start, memory_end);
    if (strncmp((char *) 0x0FFFD9, "EISA", 4) == 0)
        EISA_bus = 1;
    trap_init();
    init_IRQ();
    sched_init();
    parse_options(command_line);
#ifdef CONFIG_PROFILE
	prof_buffer = (unsigned long *) memory_start;
	prof_len = (unsigned long) &end;
	prof_len >>= 2;
	memory_start += prof_len * sizeof(unsigned long);
#endif
    memory_start = kmalloc_init(memory_start, memory_end);
    memory_start = chr_dev_init(memory_start, memory_end);
    memory_start = blk_dev_init(memory_start, memory_end);
    sti();
    calibrate_delay();
#ifdef CONFIG_INET
    memory_start = net_dev_init(memory_start, memory_end);
#endif
    memory_start = inode_init(memory_start, memory_end);
    memory_start = file_table_init(memory_start, memory_end);
    mem_init(low_memory_start, memory_start,memory_end);
    buffer_init();
    time_init();
    floppy_init();
    sock_init();
#ifdef CONFIG_SYSVIPC
	ipc_init();
#endif
    sti();
    // there is no 387
    if (!hard_math) {
		unsigned short control_word;

		printk("Checking 386/387 coupling... ");
		timer_table[COPRO_TIMER].expires = jiffies+50;
		timer_table[COPRO_TIMER].fn = copro_timeout;
		timer_active |= 1<<COPRO_TIMER;
		__asm__("clts ; fninit ; fnstcw %0 ; fwait":"=m" (*&control_word));
		control_word &= 0xffc0;
		__asm__("fldcw %0 ; fwait": :"m" (*&control_word));
		outb_p(inb_p(0x21) | (1 << 2), 0x21);
		__asm__("fldz ; fld1 ; fdiv %st,%st(1) ; fwait");
		timer_active &= ~(1<<COPRO_TIMER);
		if (!fpu_error)
			printk("Ok, fpu using %s error reporting.\n",
				ignore_irq13?"exception 16":"irq13");
	}
#ifndef CONFIG_MATH_EMULATION
	else {
		printk("No coprocessor found and no math emulation present.\n");
		printk("Giving up.\n");
		for (;;) ;
	}
#endif
    system_utsname.machine[1] = '0' + x86;
    printk("%s", linux_banner);

    move_to_user_mode();
    if (!fork()) {
        init();
    }
    for(;;) {
        idle();
    };
}

static int printf(const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
	write(1, printbuf, i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

void init(void) {
    int pid, i;
    setup((void *) &drive_info);
    sprintf(term, "TERM=con%dx%d", ORIG_VIDEO_COLS, ORIG_VIDEO_LINES);
    (void) open("/dev/tty1", O_RDWR, 0);
    (void) dup(0);
	(void) dup(0);
    printf("............it is ok\n");
    for(;;);
}