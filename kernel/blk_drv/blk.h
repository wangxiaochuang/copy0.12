#ifndef _BLK_H
#define _BLK_H

#define NR_BLK_DEV	7

#define NR_REQUEST	32

struct request {
    int dev;        /* -1 if no request */
    int cmd;
    int errors;
    unsigned long sector;
    unsigned long nr_sectors;
    char *buffer;
    struct task_struct *waiting;
    struct buffer_head *bh;
    struct request *next;
};

/**
 * 1. 读优先
 * 2. 设备号小优先
 * 3. 扇区号小优先
 **/
#define IN_ORDER(s1,s2) \
((s1)->cmd < (s2)->cmd || ((s1)->cmd == (s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector))))

struct blk_dev_struct {
    void (*request_fn) (void);
    struct request *current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
extern struct task_struct * wait_for_request;

extern int * blk_size[NR_BLK_DEV];

#ifdef MAJOR_NR
#if (MAJOR_NR == 1)
	#define DEVICE_NAME "ramdisk"
	#define DEVICE_REQUEST do_rd_request
	#define DEVICE_NR(device) ((device) & 7)
	#define DEVICE_ON(device) 
	#define DEVICE_OFF(device)
#elif (MAJOR_NR == 2)
#elif (MAJOR_NR == 3)
    #define DEVICE_NAME "harddisk"
    #define DEVICE_INTR do_hd
    #define DEVICE_TIMEOUT hd_timeout
    #define DEVICE_REQUEST do_hd_request
    #define DEVICE_NR(device) (MINOR(device) / 5)
    #define DEVICE_ON(device)
    #define DEVICE_OFF(device)
#elif
    #error "unknown blk device"
#endif

#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
#ifdef DEVICE_TIMEOUT
    int DEVICE_TIMEOUT = 0;
    #define SET_INTR(x) (DEVICE_INTR = (x),DEVICE_TIMEOUT = 200)
#else
    #define SET_INTR(x) (DEVICE_INTR = (x))
#endif
static void (DEVICE_REQUEST)(void);

static void unlock_buffer(struct buffer_head * bh) {
	if (!bh->b_lock)
		printk(DEVICE_NAME ": free buffer being unlocked\n");
	bh->b_lock=0;
	wake_up(&bh->b_wait);
}

static void end_request(int uptodate) {
	DEVICE_OFF(CURRENT->dev);
	if (CURRENT->bh) {
		CURRENT->bh->b_uptodate = uptodate;
		unlock_buffer(CURRENT->bh);
	}
	if (!uptodate) {
		printk(DEVICE_NAME " I/O error\n\r");
		printk("dev %04x, block %d\n\r",CURRENT->dev,
			CURRENT->bh->b_blocknr);
	}
	wake_up(&CURRENT->waiting);
	wake_up(&wait_for_request);
	CURRENT->dev = -1;
	CURRENT = CURRENT->next;
}

#ifdef DEVICE_TIMEOUT
    #define CLEAR_DEVICE_TIMEOUT DEVICE_TIMEOUT = 0;
#else
    #define CLEAR_DEVICE_TIMEOUT
#endif

#ifdef DEVICE_INTR
    #define CLEAR_DEVICE_INTR DEVICE_INTR = 0;
#else
    #define CLEAR_DEVICE_INTR
#endif

#define INIT_REQUEST \
repeat: \
	if (!CURRENT) {\
		CLEAR_DEVICE_INTR \
		CLEAR_DEVICE_TIMEOUT \
		return; \
	} \
	if (MAJOR(CURRENT->dev) != MAJOR_NR) \
		panic(DEVICE_NAME ": request list destroyed"); \
	if (CURRENT->bh) { \
		if (!CURRENT->bh->b_lock) \
			panic(DEVICE_NAME ": block not locked"); \
	}

#endif /* MAJOR_NR */
#endif /* _BLK_H */