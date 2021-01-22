#include <linux/sched.h>
#include <linux/tty.h>

#include <asm/io.h>
#include <asm/system.h>

#define ORIG_X			(*(unsigned char *)0x90000)
#define ORIG_Y			(*(unsigned char *)0x90001)
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)
#define ORIG_VIDEO_LINES	((*(unsigned short *)0x9000e) & 0xff)
#define ORIG_VIDEO_EGA_AX	(*(unsigned short *)0x90008)
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)
#define ORIG_VIDEO_EGA_CX	(*(unsigned short *)0x9000c)

#define VIDEO_TYPE_MDA		0x10	/* MDA单色 */
#define VIDEO_TYPE_CGA		0x11	/* CGA */
#define VIDEO_TYPE_EGAM		0x20	/* EGA单色 */
#define VIDEO_TYPE_EGAC		0x21	/* EGA彩色 */

#define NPAR 16

int NR_CONSOLES = 0;

static unsigned char	video_type;
static unsigned long	video_num_columns;  /* 列数 */
static unsigned long	video_mem_base;     /* 显存开始地址 */
static unsigned long	video_mem_term;     /* 显存末端地址 */
static unsigned long	video_size_row;     /* 每行占用的字节数 */
static unsigned long	video_num_lines;    /* 屏幕显示行数 */
static unsigned char	video_page;         /* 初始页面 */
static unsigned short	video_port_reg;
static unsigned short	video_port_val;
static int can_do_colour = 0;

static struct {
	unsigned short	vc_video_erase_char;	
	unsigned char	vc_attr;
	unsigned char	vc_def_attr;
	int		        vc_bold_attr;
	unsigned long	vc_ques;
	unsigned long	vc_state;
	unsigned long	vc_restate;
	unsigned long	vc_checkin;
	unsigned long	vc_origin;		/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_scr_end;		/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_pos;
	unsigned long	vc_x,vc_y;
	unsigned long	vc_top,vc_bottom;
	unsigned long	vc_npar,vc_par[NPAR];
	unsigned long	vc_video_mem_start;	/* Start of video RAM		*/
	unsigned long	vc_video_mem_end;	/* End of video RAM (sort of)	*/
	unsigned int	vc_saved_x;
	unsigned int	vc_saved_y;
	unsigned int	vc_iscolor;
	char *		    vc_translate;
} vc_cons [MAX_CONSOLES];

#define origin		(vc_cons[currcons].vc_origin)
#define scr_end		(vc_cons[currcons].vc_scr_end)
#define pos		(vc_cons[currcons].vc_pos)
#define top		(vc_cons[currcons].vc_top)
#define bottom		(vc_cons[currcons].vc_bottom)
#define x		(vc_cons[currcons].vc_x)
#define y		(vc_cons[currcons].vc_y)
#define state		(vc_cons[currcons].vc_state)
#define restate		(vc_cons[currcons].vc_restate)
#define checkin		(vc_cons[currcons].vc_checkin)
#define ques		(vc_cons[currcons].vc_ques)
#define attr		(vc_cons[currcons].vc_attr)
#define translate	(vc_cons[currcons].vc_translate)
#define video_mem_start	(vc_cons[currcons].vc_video_mem_start)
#define video_mem_end	(vc_cons[currcons].vc_video_mem_end)
#define def_attr	(vc_cons[currcons].vc_def_attr)
#define video_erase_char  (vc_cons[currcons].vc_video_erase_char)
#define iscolor		(vc_cons[currcons].vc_iscolor)

// 用于复位黑屏操作计数器
int blankinterval = 0;
int blankcount = 0;

#define RESPONSE "\033[?1;2c"

static char * translations[] = {
/* normal 7-bit ascii */
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~ ",
/* vt100 graphics */
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^ "
	"\004\261\007\007\007\007\370\361\007\007\275\267\326\323\327\304"
	"\304\304\304\304\307\266\320\322\272\363\362\343\\007\234\007 "
};
#define NORM_TRANS (translations[0])
#define GRAF_TRANS (translations[1])

static inline void gotoxy(int currcons, int new_x, unsigned int new_y) {
	if (new_x > video_num_columns || new_y >= video_num_lines)
		return;
	x = new_x;
	y = new_y;
	pos = origin + y*video_size_row + (x<<1);
}

/**
 * 设置滚屏起始地址
 **/
static inline void set_origin(int currcons) {
	if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		return;
	if (currcons != fg_console)
		return;
	cli();
    outb_p(12, video_port_reg);
    // 起始地址高字节，右移8位，再除以2，一个字符两个字节
    outb_p(0xff & ((origin - video_mem_base) >> 9), video_port_val);
    outb_p(13, video_port_reg);
    // 起始地址低字节，除以2，一个字符两个字节
    outb_p(0xff & ((origin - video_mem_base) >> 1), video_port_val);
    sti();
}

static void scrup(int currcons) {
	if (bottom<=top)
		return;
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		if (!top && bottom == video_num_lines) {
			origin += video_size_row;
			pos += video_size_row;
			scr_end += video_size_row;
			if (scr_end > video_mem_end) {
				__asm__("cld\n\t"
					"rep\n\t"
					"movsl\n\t"
					"movl video_num_columns,%1\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" ((video_num_lines-1)*video_num_columns>>1),
					"D" (video_mem_start),
					"S" (origin)
					);
				scr_end -= origin-video_mem_start;
				pos -= origin-video_mem_start;
				origin = video_mem_start;
			} else {
				__asm__("cld\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" (video_num_columns),
					"D" (scr_end-video_size_row)
					);
			}
			set_origin(currcons);
		} else {
			__asm__("cld\n\t"
				"rep\n\t"
				"movsl\n\t"
				"movl video_num_columns,%%ecx\n\t"
				"rep\n\t"
				"stosw"
				::"a" (video_erase_char),
				"c" ((bottom-top-1)*video_num_columns>>1),
				"D" (origin+video_size_row*top),
				"S" (origin+video_size_row*(top+1))
				);
		}
	}
	else		/* Not EGA/VGA */
	{
		__asm__("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*top),
			"S" (origin+video_size_row*(top+1))
			);
	}
}

static void scrdown(int currcons) {
	if (bottom <= top)
		return;
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			);
	}
	else		/* Not EGA/VGA */
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			);
	}
}

static void lf(int currcons) {
	if (y + 1 < bottom) {
		y++;
		pos += video_size_row;
		return;
	}
	scrup(currcons);
}

static void cr(int currcons) {
	pos -= x<<1;
	x=0;
}

static inline void set_cursor(int currcons) {
	blankcount = blankinterval;
	if (currcons != fg_console)
		return;
	cli();
	outb_p(14, video_port_reg);
    // 光标地址高字节，右移8位，再除以2，一个字符两个字节
	outb_p(0xff & ((pos - video_mem_base) >> 9), video_port_val);
	outb_p(15, video_port_reg);
    // 光标地址低字节，除以2，一个字符两个字节
	outb_p(0xff & ((pos - video_mem_base) >> 1), video_port_val);
	sti();
}
static inline void hide_cursor(int currcons) {
	outb_p(14, video_port_reg);
	outb_p(0xff&((scr_end-video_mem_base)>>9), video_port_val);
	outb_p(15, video_port_reg);
	outb_p(0xff&((scr_end-video_mem_base)>>1), video_port_val);
}

enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey, 
	ESsetterm, ESsetgraph };

static void init_graph_info(char **desc) {
    video_page = ORIG_VIDEO_PAGE;
    video_num_columns = ORIG_VIDEO_COLS;
    video_size_row = video_num_columns << 1;
    video_num_lines = ORIG_VIDEO_LINES;
    if (ORIG_VIDEO_MODE == 7) {         /* 单色显示卡 */
        video_mem_base = 0xb0000;
        video_port_reg = 0x3b4;
        video_port_val = 0x3b5;
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAM;
            video_mem_term = 0xb8000;
            *desc = "EGAm";
        } else {
            video_type = VIDEO_TYPE_MDA;
            video_mem_term = 0xb2000;
			*desc = "*MDA";
        }
    } else {                            /* 彩色显示卡 */
        can_do_colour = 1;
        video_mem_base = 0xb8000;
		video_port_reg	= 0x3d4;
		video_port_val	= 0x3d5;
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAC;
            video_mem_term = 0xc0000;
            *desc = "EGAc";
        } else {
            video_type = VIDEO_TYPE_CGA;
            video_mem_term = 0xba000;
			*desc = "*CGA";
        }
    }
}
void con_init(void) {
    char *display_desc = "????";
    char *display_ptr;
    init_graph_info(&display_desc);

    long video_memory = video_mem_term - video_mem_base;
    NR_CONSOLES = video_memory / (video_num_lines * video_size_row);
    if (NR_CONSOLES > MAX_CONSOLES)
        NR_CONSOLES = MAX_CONSOLES;
    if (!NR_CONSOLES)
        NR_CONSOLES = 1;
    video_memory /= NR_CONSOLES;        /* 每一个console占用的内存数 */

    display_ptr = ((char *) video_mem_base) + video_size_row - 8;   /* 四个字符8个字节 */
    while (*display_desc) {
        *display_ptr++ = *display_desc++;
        display_ptr++;
    }
    // 初始化0号console的结构体
    long base, term;
    int currcons = 0;
    blankcount = blankinterval;
    video_erase_char = 0x0720;
    base = origin = video_mem_start = video_mem_base;
	term = video_mem_end = base + video_memory;
    // 滚屏末端地址
    scr_end	= video_mem_start + video_num_lines * video_size_row;
    // 初始顶行号和底行号
	top	= 0;
	bottom = video_num_lines;

    // 显示属性，黑底白字
    attr = 0x07;
  	def_attr = 0x07;
    // 初始转义序列操作的当前和下一状态
        restate = state = ESnormal;
        checkin = 0;
    // 收到问号的标志
    ques = 0;
    iscolor = 0;
    // 字符集
    translate = NORM_TRANS;
    // 粗体字符属性标志（-1表示不用）
    vc_cons[0].vc_bold_attr = -1;

    // 将光标当前位置
    gotoxy(currcons, ORIG_X, ORIG_Y);
    // 循环设置其余console的结构体
    for (currcons = 1; currcons < NR_CONSOLES; currcons++) {
        vc_cons[currcons] = vc_cons[0];
        origin = video_mem_start = (base += video_memory);
        scr_end = origin + video_num_lines * video_size_row;
        video_mem_end = (term += video_memory);
        gotoxy(currcons, 0, 0);
    }
    update_screen();
    register unsigned char a;
	// set_trap_gate(0x21, &keyboard_interrupt);
	outb_p(inb_p(0x21)&0xfd,0x21);  /* 取消对键盘中断的屏蔽，允许IRQ1 */
    // 读取键盘端口0x61，先禁止再允许，用以复位键盘
	a = inb_p(0x61);
	outb_p(a|0x80,0x61);
	outb_p(a,0x61);
}

void update_screen(void) {
    set_origin(fg_console);
    set_cursor(fg_console);
}

static inline unsigned char new_get_fs_byte(const char * addr) {
	unsigned register char _v;
	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

void console_print(const char *b) {
    int currcons = fg_console;
    char c;

    while ((c = *(b++))) {
    // while ((c = new_get_fs_byte(b++))) {
        if (c == 10) {
            cr(currcons);
            lf(currcons);
            continue;
        }
        if (c == 13) {
            cr(currcons);
            continue;
        }
        // 如果当前光标已经达到屏幕右末端，则修正光标的正确位置
        if (x >= video_num_columns) {
            x -= video_num_columns;
            pos -= video_size_row;
            lf(currcons);
        }
        __asm__("movb %2,%%ah\n\t"
			"movw %%ax,%1\n\t"
			::"a" (c),
			"m" (*(short *)pos),
			"m" (attr)
			);
        pos += 2;
        x++;
    }
    set_cursor(currcons);
}