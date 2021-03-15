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

int blankinterval = 10*60*HZ;
static int screen_size = 0;

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
    }
}

static void get_scrmem(int currcons) {
    return;
}

static void set_scrmem(int currcons) {
    return;
}

void blank_screen(void) {
    if (console_blanked)
        return;
    timer_table[BLANK_TIMER].fn = unblank_screen;
    get_scrmem(fg_console);
}

void unblank_screen(void) {

}