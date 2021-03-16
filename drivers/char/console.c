#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kd.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/segment.h>

#include "kbd_kern.h"
#include "vt_kern.h"

#define NPAR 16

extern void vt_init(void);
extern void register_console(void (*proc)(const char *));
extern void compute_shiftstate(void);

unsigned long	video_num_columns;
unsigned long	video_num_lines;

static unsigned char	video_type;
static unsigned long	video_mem_base;
static unsigned long	video_mem_term;
static unsigned long	video_size_row;
static unsigned char	video_page;
static unsigned short	video_port_reg;
static unsigned short	video_port_val;
static int can_do_color = 0;
static int printable = 0;

static struct {
	unsigned short	vc_video_erase_char;	/* Background erase character */
	unsigned char	vc_attr;		/* Current attributes */
	unsigned char	vc_def_color;		/* Default colors */
	unsigned char	vc_color;		/* Foreground & background */
	unsigned char	vc_s_color;		/* Saved foreground & background */
	unsigned char	vc_ulcolor;		/* Colour for underline mode */
	unsigned char	vc_halfcolor;		/* Colour for half intensity mode */
	unsigned long	vc_origin;		/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_scr_end;		/* Used for EGA/VGA fast scroll	*/
	unsigned long	vc_pos;
	unsigned long	vc_x,vc_y;
	unsigned long	vc_top,vc_bottom;
	unsigned long	vc_state;
	unsigned long	vc_npar,vc_par[NPAR];
	unsigned long	vc_video_mem_start;	/* Start of video RAM		*/
	unsigned long	vc_video_mem_end;	/* End of video RAM (sort of)	*/
	unsigned long	vc_saved_x;
	unsigned long	vc_saved_y;
	/* mode flags */
	unsigned long	vc_charset	: 1;	/* Character set G0 / G1 */
	unsigned long	vc_s_charset	: 1;	/* Saved character set */
	unsigned long	vc_decscnm	: 1;	/* Screen Mode */
	unsigned long	vc_decom	: 1;	/* Origin Mode */
	unsigned long	vc_decawm	: 1;	/* Autowrap Mode */
	unsigned long	vc_deccm	: 1;	/* Cursor Visible */
	unsigned long	vc_decim	: 1;	/* Insert Mode */
	/* attribute flags */
	unsigned long	vc_intensity	: 2;	/* 0=half-bright, 1=normal, 2=bold */
	unsigned long	vc_underline	: 1;
	unsigned long	vc_blink	: 1;
	unsigned long	vc_reverse	: 1;
	unsigned long	vc_s_intensity	: 2;	/* saved rendition */
	unsigned long	vc_s_underline	: 1;
	unsigned long	vc_s_blink	: 1;
	unsigned long	vc_s_reverse	: 1;
	/* misc */
	unsigned long	vc_ques		: 1;
	unsigned long	vc_need_wrap	: 1;
	unsigned long	vc_tab_stop[5];		/* Tab stops. 160 columns. */
	unsigned char * vc_translate;
	unsigned char *	vc_G0_charset;
	unsigned char *	vc_G1_charset;
	unsigned char *	vc_saved_G0;
	unsigned char *	vc_saved_G1;
	/* additional information is in vt_kern.h */
} vc_cons [NR_CONSOLES];

unsigned short *vc_scrbuf[NR_CONSOLES];
static unsigned short * vc_scrmembuf;
static int console_blanked = 0;

#define origin		(vc_cons[currcons].vc_origin)
#define scr_end		(vc_cons[currcons].vc_scr_end)
#define pos		(vc_cons[currcons].vc_pos)
#define top		(vc_cons[currcons].vc_top)
#define bottom		(vc_cons[currcons].vc_bottom)
#define x		(vc_cons[currcons].vc_x)
#define y		(vc_cons[currcons].vc_y)
#define state		(vc_cons[currcons].vc_state)
#define npar		(vc_cons[currcons].vc_npar)
#define par		(vc_cons[currcons].vc_par)
#define ques		(vc_cons[currcons].vc_ques)
#define attr		(vc_cons[currcons].vc_attr)
#define saved_x		(vc_cons[currcons].vc_saved_x)
#define saved_y		(vc_cons[currcons].vc_saved_y)
#define translate	(vc_cons[currcons].vc_translate)
#define G0_charset	(vc_cons[currcons].vc_G0_charset)
#define G1_charset	(vc_cons[currcons].vc_G1_charset)
#define saved_G0	(vc_cons[currcons].vc_saved_G0)
#define saved_G1	(vc_cons[currcons].vc_saved_G1)
#define video_mem_start	(vc_cons[currcons].vc_video_mem_start)
#define video_mem_end	(vc_cons[currcons].vc_video_mem_end)
#define video_erase_char (vc_cons[currcons].vc_video_erase_char)	
#define decscnm		(vc_cons[currcons].vc_decscnm)
#define decom		(vc_cons[currcons].vc_decom)
#define decawm		(vc_cons[currcons].vc_decawm)
#define deccm		(vc_cons[currcons].vc_deccm)
#define decim		(vc_cons[currcons].vc_decim)
#define need_wrap	(vc_cons[currcons].vc_need_wrap)
#define color		(vc_cons[currcons].vc_color)
#define s_color		(vc_cons[currcons].vc_s_color)
#define def_color	(vc_cons[currcons].vc_def_color)
#define	foreground	(color & 0x0f)
#define background	(color & 0xf0)
#define charset		(vc_cons[currcons].vc_charset)
#define s_charset	(vc_cons[currcons].vc_s_charset)
#define	intensity	(vc_cons[currcons].vc_intensity)
#define	underline	(vc_cons[currcons].vc_underline)
#define	blink		(vc_cons[currcons].vc_blink)
#define	reverse		(vc_cons[currcons].vc_reverse)
#define	s_intensity	(vc_cons[currcons].vc_s_intensity)
#define	s_underline	(vc_cons[currcons].vc_s_underline)
#define	s_blink		(vc_cons[currcons].vc_s_blink)
#define	s_reverse	(vc_cons[currcons].vc_s_reverse)
#define	ulcolor		(vc_cons[currcons].vc_ulcolor)
#define	halfcolor	(vc_cons[currcons].vc_halfcolor)
#define tab_stop	(vc_cons[currcons].vc_tab_stop)
#define vcmode		(vt_cons[currcons].vc_mode)
#define vtmode		(vt_cons[currcons].vt_mode)
#define vtpid		(vt_cons[currcons].vt_pid)
#define vtnewvt		(vt_cons[currcons].vt_newvt)

#define set_kbd(x) set_vc_kbd_mode(kbd_table+currcons,x)
#define clr_kbd(x) clr_vc_kbd_mode(kbd_table+currcons,x)
#define is_kbd(x) vc_kbd_mode(kbd_table+currcons,x)

#define decarm		VC_REPEAT
#define decckm		VC_CKMODE
#define kbdapplic	VC_APPLIC
#define kbdraw		VC_RAW
#define lnm		VC_CRLF

int blankinterval = 10*60*HZ;
static int screen_size = 0;

static unsigned char * translations[] = {
/* 8-bit Latin-1 mapped to the PC character set: '\0' means non-printable */
(unsigned char *)
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\376\0\0\0\0\0"
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\377\255\233\234\376\235\174\025\376\376\246\256\252\055\376\376"
	"\370\361\375\376\376\346\024\371\376\376\247\257\254\253\376\250"
	"\376\376\376\376\216\217\222\200\376\220\376\376\376\376\376\376"
	"\376\245\376\376\376\376\231\376\350\376\376\376\232\376\376\341"
	"\205\240\203\376\204\206\221\207\212\202\210\211\215\241\214\213"
	"\376\244\225\242\223\376\224\366\355\227\243\226\201\376\376\230",
/* vt100 graphics */
(unsigned char *)
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\376\0\0\0\0\0"
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^ "
	"\004\261\007\007\007\007\370\361\007\007\331\277\332\300\305\304"
	"\304\304\137\137\303\264\301\302\263\363\362\343\330\234\007\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\377\255\233\234\376\235\174\025\376\376\246\256\252\055\376\376"
	"\370\361\375\376\376\346\024\371\376\376\247\257\254\253\376\250"
	"\376\376\376\376\216\217\222\200\376\220\376\376\376\376\376\376"
	"\376\245\376\376\376\376\231\376\376\376\376\376\232\376\376\341"
	"\205\240\203\376\204\206\221\207\212\202\210\211\215\241\214\213"
	"\376\244\225\242\223\376\224\366\376\227\243\226\201\376\376\230",
/* IBM graphics: minimal translations (BS, CR, LF, LL, SO, SI and ESC) */
(unsigned char *)
	"\000\001\002\003\004\005\006\007\000\011\000\013\000\000\000\000"
	"\020\021\022\023\024\025\026\027\030\031\032\000\034\035\036\037"
	"\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057"
	"\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077"
	"\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117"
	"\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137"
	"\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157"
	"\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177"
	"\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
	"\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
	"\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
	"\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
	"\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317"
	"\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
	"\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357"
	"\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377",
 /* USER: customizable mappings, initialized as the previous one (IBM) */
(unsigned char *)
	"\000\001\002\003\004\005\006\007\010\011\000\013\000\000\016\017"
	"\020\021\022\023\024\025\026\027\030\031\032\000\034\035\036\037"
	"\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057"
	"\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077"
	"\100\101\102\103\104\105\106\107\110\111\112\113\114\115\116\117"
	"\120\121\122\123\124\125\126\127\130\131\132\133\134\135\136\137"
	"\140\141\142\143\144\145\146\147\150\151\152\153\154\155\156\157"
	"\160\161\162\163\164\165\166\167\170\171\172\173\174\175\176\177"
	"\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217"
	"\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237"
	"\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257"
	"\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277"
	"\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317"
	"\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337"
	"\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357"
	"\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377"
};

#define NORM_TRANS (translations[0])
#define GRAF_TRANS (translations[1])
#define NULL_TRANS (translations[2])
#define USER_TRANS (translations[3])

static void gotoxy(int currcons, int new_x, int new_y) {
    int max_y;

    if (new_x < 0)
        x = 0;
    else {
        if (new_x >= video_num_columns)
            x = video_num_columns - 1;
        else
            x = new_x;
    }
    if (decom) {
        new_y += top;
        max_y = bottom;
    } else {
        max_y = video_num_lines;
    }
    if (new_y < 0)
        y = 0;
    else {
        if (new_y >= max_y)
            y = max_y - 1;
        else
            y = new_y;
    }
    pos = origin + y * video_size_row + (x << 1);
    need_wrap = 0;
}

/*
 * *Very* limited hardware scrollback support..
 */
static unsigned short __real_origin;
static unsigned short __origin;

static inline void __set_origin(unsigned short offset) {
	unsigned long flags;
#ifdef CONFIG_SELECTION
	clear_selection();
#endif /* CONFIG_SELECTION */
	save_flags(flags); cli();
	__origin = offset;
	outb_p(12, video_port_reg);
	outb_p(offset >> 8, video_port_val);
	outb_p(13, video_port_reg);
	outb_p(offset, video_port_val);
	restore_flags(flags);
}

static void set_origin(int currcons) {
    if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		return;
	if (currcons != fg_console || console_blanked || vcmode == KD_GRAPHICS)
		return;
	__real_origin = (origin-video_mem_base) >> 1;
	__set_origin(__real_origin);
}

static inline void hide_cursor(void) {
    outb_p(14, video_port_reg);
	outb_p(0xff&((video_mem_term-video_mem_base)>>9), video_port_val);
	outb_p(15, video_port_reg);
	outb_p(0xff&((video_mem_term-video_mem_base)>>1), video_port_val);
}

static inline void set_cursor(int currcons) {
    unsigned long flags;

	if (currcons != fg_console || console_blanked || vcmode == KD_GRAPHICS)
		return;
	if (__real_origin != __origin)
		set_origin(__real_origin);
	save_flags(flags); cli();
	if (deccm) {
		outb_p(14, video_port_reg);
		outb_p(0xff&((pos-video_mem_base)>>9), video_port_val);
		outb_p(15, video_port_reg);
		outb_p(0xff&((pos-video_mem_base)>>1), video_port_val);
	} else
		hide_cursor();
	restore_flags(flags);
}

static void scrup(int currcons, unsigned int t, unsigned int b)
{
	int hardscroll = 1;

	if (b > video_num_lines || t >= b)
		return;
	if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		hardscroll = 0;
	else if (t || b != video_num_lines)
		hardscroll = 0;
	if (hardscroll) {
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
				: /* no output */
				:"a" (video_erase_char),
				"c" ((video_num_lines-1)*video_num_columns>>1),
				"D" (video_mem_start),
				"S" (origin));
			scr_end -= origin-video_mem_start;
			pos -= origin-video_mem_start;
			origin = video_mem_start;
		} else {
			__asm__("cld\n\t"
				"rep\n\t"
				"stosw"
				: /* no output */
				:"a" (video_erase_char),
				"c" (video_num_columns),
				"D" (scr_end-video_size_row));
		}
		set_origin(currcons);
	} else {
		__asm__("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			"movl video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			: /* no output */
			:"a" (video_erase_char),
			"c" ((b-t-1)*video_num_columns>>1),
			"D" (origin+video_size_row*t),
			"S" (origin+video_size_row*(t+1)));
	}
}

static void scrdown(int currcons, unsigned int t, unsigned int b)
{
	if (b > video_num_lines || t >= b)
		return;
	__asm__("std\n\t"
		"rep\n\t"
		"movsl\n\t"
		"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
		"movl video_num_columns,%%ecx\n\t"
		"rep\n\t"
		"stosw\n\t"
		"cld"
		: /* no output */
		:"a" (video_erase_char),
		"c" ((b-t-1)*video_num_columns>>1),
		"D" (origin+video_size_row*b-4),
		"S" (origin+video_size_row*(b-1)-4));
}

static void lf(int currcons) {
	if (y+1<bottom) {
		y++;
		pos += video_size_row;
		return;
	} else 
		scrup(currcons,top,bottom);
	need_wrap = 0;
}

static inline void cr(int currcons) {
	pos -= x<<1;
	need_wrap = x = 0;
}

static void csi_J(int currcons, int vpar) {
	unsigned long count;
	unsigned long start;

	switch (vpar) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = ((pos-origin)>>1)+1;
			start = origin;
			break;
		case 2: /* erase whole display */
			count = video_num_columns * video_num_lines;
			start = origin;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		: /* no output */
		:"c" (count),
		"D" (start),"a" (video_erase_char));
	need_wrap = 0;
}

static void update_attr(int currcons) {
    attr = color;
    if (can_do_color) {
        if (underline)
            attr = (attr & 0xf0) | ulcolor;
        else if (intensity == 0)
            attr = (attr & 0xf0) | halfcolor;
    }
    if (reverse ^ decscnm)
        attr = (attr & 0x88) | (((attr >> 4) | (attr << 4)) & 0x77);
    if (blink)
        attr ^= 0x80;
    if (intensity == 2)
        attr ^= 0x08;
    if (!can_do_color) {
		if (underline)
			attr = (attr & 0xf8) | 0x01;
		else if (intensity == 0)
			attr = (attr & 0xf0) | 0x08;
	}
	if (decscnm)
		video_erase_char = (((color & 0x88) | (((color >> 4) | (color << 4)) & 0x77)) << 8) | ' ';
	else
		video_erase_char = (color << 8) | ' ';
}

static void default_attr(int currcons) {
    intensity = 1;
    underline = 0;
    reverse = 0;
    blink = 0;
    color = def_color;
}

static void save_cur(int currcons) {
	saved_x		= x;
	saved_y		= y;
	s_intensity	= intensity;
	s_underline	= underline;
	s_blink		= blink;
	s_reverse	= reverse;
	s_charset	= charset;
	s_color		= color;
	saved_G0	= G0_charset;
	saved_G1	= G1_charset;
}

static void restore_cur(int currcons) {
	gotoxy(currcons, saved_x, saved_y);
	intensity	= s_intensity;
	underline	= s_underline;
	blink		= s_blink;
	reverse		= s_reverse;
	charset		= s_charset;
	color		= s_color;
	G0_charset	= saved_G0;
	G1_charset	= saved_G1;
	translate	= charset ? G1_charset : G0_charset;
	update_attr(currcons);
	need_wrap = 0;
}

enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey, 
	EShash, ESsetG0, ESsetG1, ESignore };

static void reset_terminal(int currcons, int do_clear) {
    top = 0;
    bottom = video_num_lines;
    state = ESnormal;
    ques = 0;
    translate = NORM_TRANS;
    G0_charset = NORM_TRANS;
    G1_charset = GRAF_TRANS;
    charset = 0;
    need_wrap = 0;

    decscnm = 0;
    decom = 0;
    decawm = 1;
    deccm = 1;
    decim = 0;

    set_kbd(decarm);
    clr_kbd(decckm);
    clr_kbd(kbdapplic);
    clr_kbd(lnm);
    kbd_table[currcons].lockstate = 0;
    kbd_table[currcons].ledstate = kbd_table[currcons].default_ledstate;
    set_leds();

    default_attr(currcons);
	update_attr(currcons);
	tab_stop[0]	= 0x01010100;
	tab_stop[1]	=
	tab_stop[2]	=
	tab_stop[3]	=
	tab_stop[4]	= 0x01010101;
	if (do_clear) {
		gotoxy(currcons, 0, 0);
		csi_J(currcons, 2);
		save_cur(currcons);
	}
}

void console_print(const char * b) {
    int currcons = fg_console;
	unsigned char c;

	if (!printable || currcons<0 || currcons>=NR_CONSOLES)
		return;
	while ((c = *(b++)) != 0) {
		if (c == 10 || c == 13 || need_wrap) {
			if (c != 13)
				lf(currcons);
			cr(currcons);
			if (c == 10 || c == 13)
				continue;
		}
		*(unsigned short *) pos = (attr << 8) + c;
		if (x == video_num_columns - 1) {
			need_wrap = 1;
			continue;
		}
		x++;
		pos+=2;
	}
	set_cursor(currcons);
	if (vt_cons[fg_console].vc_mode == KD_GRAPHICS)
		return;
	timer_active &= ~(1<<BLANK_TIMER);
	if (console_blanked) {
		timer_table[BLANK_TIMER].expires = 0;
		timer_active |= 1<<BLANK_TIMER;
	} else if (blankinterval) {
		timer_table[BLANK_TIMER].expires = jiffies + blankinterval;
		timer_active |= 1<<BLANK_TIMER;
	}
}

long con_init(long kmem_start) {
    char *display_desc = "????";
    int currcons = 0;
    long base;
    int orig_x = ORIG_X;
    int orig_y = ORIG_Y;

    vc_scrmembuf = (unsigned short *) kmem_start;
    video_num_columns = ORIG_VIDEO_COLS;
    video_size_row = video_num_columns * 2;
    video_num_lines = ORIG_VIDEO_LINES;
    video_page = ORIG_VIDEO_PAGE;
    screen_size = (video_num_lines * video_size_row);
    kmem_start += NR_CONSOLES * screen_size;
    timer_table[BLANK_TIMER].fn = blank_screen;
    timer_table[BLANK_TIMER].expires = 0;
    if (blankinterval) {
        timer_table[BLANK_TIMER].expires = jiffies + blankinterval;
        timer_active |= 1 << BLANK_TIMER;
    }

    if (ORIG_VIDEO_MODE == 7) {
        video_mem_base = 0xb0000;
        video_port_reg = 0x3b4;
        video_port_val = 0x3b5;
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAM;
            video_mem_term = 0xb8000;
            display_desc = "EGA+";
        } else {
            video_type = VIDEO_TYPE_MDA;
            video_mem_term = 0xb2000;
            display_desc = "*MDA";
        }
    } else {
        can_do_color = 1;
        video_mem_base = 0xb8000;
        video_port_reg = 0x3d4;
        video_port_val = 0x3d5;
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAC;
            video_mem_term = 0xc0000;
            display_desc = "EGA+";
        } else {
            video_type = VIDEO_TYPE_CGA;
            video_mem_term = 0xba000;
            display_desc = "*CGA";
        }
    }
    base = (long) vc_scrmembuf;
    for (currcons = 0; currcons < NR_CONSOLES; currcons++) {
        pos = origin = video_mem_start = base;
        scr_end = video_mem_end = (base += screen_size);
        vc_scrbuf[currcons] = (unsigned short *) origin;
        vcmode = KD_TEXT;
        vtmode.mode = VT_AUTO;
        vtmode.waitv = 0;
        vtmode.relsig = 0;
        vtmode.acqsig = 0;
        vtmode.frsig = 0;
        vtpid = -1;
        vtnewvt = -1;
        clr_kbd(kbdraw);
        def_color = 0x07;       // white
        ulcolor = 0x0f;         // bold white
        halfcolor = 0x08;       // grey
        reset_terminal(currcons, currcons);
    }
    currcons = fg_console = 0;

    video_mem_start = video_mem_base;
    video_mem_end = video_mem_term;
    origin = video_mem_start;
    scr_end = video_mem_start + video_num_lines * video_size_row;
    gotoxy(currcons, 0, 0);
    save_cur(currcons);
    gotoxy(currcons, orig_x, orig_y);
    update_screen(fg_console);
    printable = 1;
    printk("Console: %s %s %ldx%ld, %d virtual consoles\n",
		can_do_color ? "colour" : "mono",
		display_desc,
		video_num_columns, video_num_lines,
		NR_CONSOLES);
    register_console(console_print);
    return kmem_start;
}

void kbdsave(int new_console) {}

static void get_scrmem(int currcons) {
    memcpy((void *) vc_scrbuf[currcons], (void *) origin, screen_size);
    video_mem_start = (unsigned long) vc_scrbuf[currcons];
    origin = video_mem_start;
    scr_end = video_mem_end = video_mem_start + screen_size;
    pos = origin = y * video_size_row + (x << 1);
}

static void set_scrmem(int currcons) {
    video_mem_start = video_mem_base;
    video_mem_end = video_mem_term;
    origin = video_mem_start;
    scr_end = video_mem_start + screen_size;
    pos = origin + y * video_size_row + (x << 1);
    memcpy((void *) video_mem_base, (void *) vc_scrbuf[fg_console], screen_size);
}

void blank_screen(void) {
    if (console_blanked)
        return;
    timer_table[BLANK_TIMER].fn = unblank_screen;
    get_scrmem(fg_console);
}

void unblank_screen(void) {

}

void update_screen(int new_console) {
    static int lock = 0;
    if (new_console == fg_console || lock)
        return;
    lock = 1;
    kbdsave(new_console);
    get_scrmem(fg_console);
    fg_console = new_console;
    set_scrmem(fg_console);
    set_origin(fg_console);
    set_cursor(new_console);
    set_leds();
    compute_shiftstate();
    lock = 0;
}