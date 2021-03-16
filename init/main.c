#include <asm/system.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/head.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/ioport.h>

extern char edata, end;
asmlinkage void lcall7(void);
struct desc_struct default_ldt;

static char printbuf[1024];

extern int console_loglevel;

extern char empty_zero_page[PAGE_SIZE];
extern void init_IRQ(void);
extern long kmalloc_init (long,long);
extern long blk_dev_init(long,long);
extern long chr_dev_init(long,long);
extern unsigned long simple_strtoul(const char *,char **,unsigned int);

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
    for(;;);
}