#include <asm/system.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/head.h>
#include <linux/string.h>
#include <linux/fs.h>

extern char edata, end;
asmlinkage void lcall7(void);
struct desc_struct default_ldt;

extern char empty_zero_page[PAGE_SIZE];
extern void init_IRQ(void);
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

struct drive_info_struct { char dummy[32]; } drive_info;
struct screen_info screen_info;

unsigned char aux_device_present;
int ramdisk_size;
int root_mountflags = 0;

static char command_line[80] = { 0, };

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
    for(;;);
}