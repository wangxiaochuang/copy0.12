#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/system.h>

/* 该符号常量定义终端IO结构的默认数据 */
#define DEF_TERMIOS 		                                \
	(struct termios) { 		                                \
	        ICRNL, 			                                \
        	OPOST | ONLCR,                                  \
	        0,                                              \
	        IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE, \
	        0,                                              \
	        INIT_C_CC                                       \
    }


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
#define npar		(vc_cons[currcons].vc_npar)
#define par		(vc_cons[currcons].vc_par)
#define ques		(vc_cons[currcons].vc_ques)
#define attr		(vc_cons[currcons].vc_attr)
#define saved_x		(vc_cons[currcons].vc_saved_x)
#define saved_y		(vc_cons[currcons].vc_saved_y)
#define translate	(vc_cons[currcons].vc_translate)
#define video_mem_start	(vc_cons[currcons].vc_video_mem_start)
#define video_mem_end	(vc_cons[currcons].vc_video_mem_end)
#define def_attr	(vc_cons[currcons].vc_def_attr)
#define video_erase_char  (vc_cons[currcons].vc_video_erase_char)
#define iscolor		(vc_cons[currcons].vc_iscolor)

// 用于复位黑屏操作计数器
int blankinterval = 0;
int blankcount = 0;

static void sysbeep(void);

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

static void ri(int currcons) {
	if (y>top) {
		y--;
		pos -= video_size_row;
		return;
	}
	scrdown(currcons);
}

static void cr(int currcons) {
	pos -= x<<1;
	x=0;
}

static void del(int currcons) {
	if (x) {
		pos -= 2;
		x--;
		*(unsigned short *)pos = video_erase_char;
	}
}

static void csi_J(int currcons, int vpar) {
	long count;
	long start;

	switch (vpar) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = (pos-origin)>>1;
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
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		);
}

static void csi_K(int currcons, int vpar) {
	long count;
	long start;

	switch (vpar) {
		case 0:	/* erase from cursor to end of line */
			if (x>=video_num_columns)
				return;
			count = video_num_columns-x;
			start = pos;
			break;
		case 1:	/* erase from start of line to cursor */
			start = pos - (x<<1);
			count = (x<video_num_columns)?x:video_num_columns;
			break;
		case 2: /* erase whole line */
			start = pos - (x<<1);
			count = video_num_columns;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		);
}

void csi_m(int currcons ) {
	int i;

	for (i=0;i<=npar;i++)
		switch (par[i]) {
			case 0: attr=def_attr;break;  /* default */
			case 1: attr=(iscolor?attr|0x08:attr|0x0f);break;  /* bold */
			/*case 4: attr=attr|0x01;break;*/  /* underline */
			case 4: /* bold */ 
			  if (!iscolor)
			    attr |= 0x01;
			  else
			  { /* check if forground == background */
			    if (vc_cons[currcons].vc_bold_attr != -1)
			      attr = (vc_cons[currcons].vc_bold_attr&0x0f)|(0xf0&(attr));
			    else
			    { short newattr = (attr&0xf0)|(0xf&(~attr));
			      attr = ((newattr&0xf)==((attr>>4)&0xf)? 
			        (attr&0xf0)|(((attr&0xf)+1)%0xf):
			        newattr);
			    }    
			  }
			  break;
			case 5: attr=attr|0x80;break;  /* blinking */
			case 7: attr=(attr<<4)|(attr>>4);break;  /* negative */
			case 22: attr=attr&0xf7;break; /* not bold */ 
			case 24: attr=attr&0xfe;break;  /* not underline */
			case 25: attr=attr&0x7f;break;  /* not blinking */
			case 27: attr=def_attr;break; /* positive image */
			case 39: attr=(attr & 0xf0)|(def_attr & 0x0f); break;
			case 49: attr=(attr & 0x0f)|(def_attr & 0xf0); break;
			default:
			  if (!can_do_colour)
			    break;
			  iscolor = 1;
			  if ((par[i]>=30) && (par[i]<=38))
			    attr = (attr & 0xf0) | (par[i]-30);
			  else  /* Background color */
			    if ((par[i]>=40) && (par[i]<=48))
			      attr = (attr & 0x0f) | ((par[i]-40)<<4);
			    else
				break;
		}
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

static void respond(int currcons, struct tty_struct * tty) {
	char * p = RESPONSE;

	cli();
	while (*p) {
		PUTCH(*p,tty->read_q);
		p++;
	}
	sti();
	copy_to_cooked(tty);
}

static void insert_char(int currcons) {
	int i=x;
	unsigned short tmp, old = video_erase_char;
	unsigned short * p = (unsigned short *) pos;

	while (i++<video_num_columns) {
		tmp=*p;
		*p=old;
		old=tmp;
		p++;
	}
}

static void insert_line(int currcons) {
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrdown(currcons);
	top=oldtop;
	bottom=oldbottom;
}

static void delete_char(int currcons) {
	int i;
	unsigned short * p = (unsigned short *) pos;

	if (x>=video_num_columns)
		return;
	i = x;
	while (++i < video_num_columns) {
		*p = *(p+1);
		p++;
	}
	*p = video_erase_char;
}

static void delete_line(int currcons) {
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrup(currcons);
	top=oldtop;
	bottom=oldbottom;
}

static void csi_at(int currcons, unsigned int nr) {
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_char(currcons);
}

static void csi_L(int currcons, unsigned int nr) {
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_line(currcons);
}

static void csi_P(int currcons, unsigned int nr) {
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		delete_char(currcons);
}

static void csi_M(int currcons, unsigned int nr) {
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_line(currcons);
}

static void save_cur(int currcons) {
	saved_x=x;
	saved_y=y;
}

static void restore_cur(int currcons) {
	gotoxy(currcons,saved_x, saved_y);
}

enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey, 
	ESsetterm, ESsetgraph };

void con_write(struct tty_struct * tty) {
	int nr;
	char c;
	int currcons;
     
	currcons = tty - tty_table;
	if ((currcons >= MAX_CONSOLES) || (currcons < 0))
		panic("con_write: illegal tty");
 	   
	nr = CHARS(tty->write_q);
	while (nr--) {
		if (tty->stopped)
			break;
		GETCH(tty->write_q,c);
		// 24: cancel->Ctrl+X, 26: substitude-> Ctrl+Z
		if (c == 24 || c == 26)
			state = ESnormal;
		switch(state) {
			case ESnormal:
				// 普通字符
				if (c>31 && c<127) {
					if (x >= video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf(currcons);
					}
					__asm__("movb %2,%%ah\n\t"
						"movw %%ax,%1\n\t"
						::"a" (translate[c-32]),
						"m" (*(short *)pos),
						"m" (attr)
						);
					pos += 2;
					x++;
				// ESC
				} else if (c==27)
					state=ESesc;
				else if (c==10 || c==11 || c==12)
					lf(currcons);
				else if (c==13)
					cr(currcons);
				// del
				else if (c==ERASE_CHAR(tty))
					del(currcons);
				// backspace
				else if (c==8) {
					if (x) {
						x--;
						pos -= 2;
					}
				// 水平制表符，光标移动到8的倍数上，超出则移到下一行并恢复c
				} else if (c==9) {
					c=8-(x&7);
					x += c;
					pos += c<<1;
					if (x>video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf(currcons);
					}
					c=9;
				// BEL 响铃
				} else if (c==7)
					sysbeep();
			  	else if (c == 14)				/* SO 换出，使用G1 */
			  		translate = GRAF_TRANS;
			  	else if (c == 15)     			/* SI 换出，使用G0 */
					translate = NORM_TRANS;
				break;
			// ESnormal下收到转义字符ESC，则转到本状态，处理完成后默认将是ESnormal状态
			case ESesc:
				state = ESnormal;
				switch (c)
				{
				  case '[':
					state=ESsquare;
					break;
				  case 'E':
					gotoxy(currcons,0,y+1);
					break;
				  case 'M':
					ri(currcons);
					break;
				  case 'D':
					lf(currcons);
					break;
				  case 'Z':							// 设备属性查询
					respond(currcons,tty);
					break;
				  case '7':
					save_cur(currcons);
					break;
				  case '8':
					restore_cur(currcons);
					break;
				  case '(':  case ')':
				    	state = ESsetgraph;		
					break;
				  case 'P':
				    	state = ESsetterm;  
				    	break;
				  case '#':
				  	state = -1;
				  	break;  	
				  case 'c':
					tty->termios = DEF_TERMIOS;
				  	state = restate = ESnormal;
					checkin = 0;
					top = 0;
					bottom = video_num_lines;
					break;
				 /* case '>':   Numeric keypad */
				 /* case '=':   Appl. keypad */
				}	
				break;
			case ESsquare:
				for(npar=0;npar<NPAR;npar++)
					par[npar]=0;
				npar=0;
				state=ESgetpars;
				if (c =='[')  /* Function key */
				{ state=ESfunckey;
				  break;
				}  
				if ((ques=(c=='?')))
					break;
			case ESgetpars:
				if (c==';' && npar<NPAR-1) {
					npar++;
					break;
				} else if (c>='0' && c<='9') {
					par[npar]=10*par[npar]+c-'0';
					break;
				} else state=ESgotpars;
			case ESgotpars:
				state = ESnormal;
				if (ques)
				{ ques =0;
				  break;
				}  
				switch(c) {
					case 'G': case '`':
						if (par[0]) par[0]--;
						gotoxy(currcons,par[0],y);
						break;
					case 'A':
						if (!par[0]) par[0]++;
						gotoxy(currcons,x,y-par[0]);
						break;
					case 'B': case 'e':
						if (!par[0]) par[0]++;
						gotoxy(currcons,x,y+par[0]);
						break;
					case 'C': case 'a':
						if (!par[0]) par[0]++;
						gotoxy(currcons,x+par[0],y);
						break;
					case 'D':
						if (!par[0]) par[0]++;
						gotoxy(currcons,x-par[0],y);
						break;
					case 'E':
						if (!par[0]) par[0]++;
						gotoxy(currcons,0,y+par[0]);
						break;
					case 'F':
						if (!par[0]) par[0]++;
						gotoxy(currcons,0,y-par[0]);
						break;
					case 'd':
						if (par[0]) par[0]--;
						gotoxy(currcons,x,par[0]);
						break;
					case 'H': case 'f':
						if (par[0]) par[0]--;
						if (par[1]) par[1]--;
						gotoxy(currcons,par[1],par[0]);
						break;
					case 'J':
						csi_J(currcons,par[0]);
						break;
					case 'K':
						csi_K(currcons,par[0]);
						break;
					case 'L':
						csi_L(currcons,par[0]);
						break;
					case 'M':
						csi_M(currcons,par[0]);
						break;
					case 'P':
						csi_P(currcons,par[0]);
						break;
					case '@':
						csi_at(currcons,par[0]);
						break;
					case 'm':
						csi_m(currcons);
						break;
					case 'r':
						if (par[0]) par[0]--;
						if (!par[1]) par[1] = video_num_lines;
						if (par[0] < par[1] &&
						    par[1] <= video_num_lines) {
							top=par[0];
							bottom=par[1];
						}
						break;
					case 's':
						save_cur(currcons);
						break;
					case 'u':
						restore_cur(currcons);
						break;
					case 'l': /* blank interval */
					case 'b': /* bold attribute */
						  if (!((npar >= 2) &&
						  ((par[1]-13) == par[0]) && 
						  ((par[2]-17) == par[0]))) 
						    break;
						if ((c=='l')&&(par[0]>=0)&&(par[0]<=60))
						{  
						  blankinterval = HZ*60*par[0];
						  blankcount = blankinterval;
						}
						if (c=='b')
						  vc_cons[currcons].vc_bold_attr
						    = par[0];
				}
				break;
			case ESfunckey:
				state = ESnormal;
				break;
			case ESsetterm:  /* Setterm functions. */
				state = ESnormal;
				if (c == 'S') {
					def_attr = attr;
					video_erase_char = (video_erase_char&0x0ff) | (def_attr<<8);
				} else if (c == 'L')
					; /*linewrap on*/
				else if (c == 'l')
					; /*linewrap off*/
				break;
			case ESsetgraph:
				state = ESnormal;
				if (c == '0')
					translate = GRAF_TRANS;
				else if (c == 'B')
					translate = NORM_TRANS;
				break;
			default:
				state = ESnormal;
		}
	}
	set_cursor(currcons);
}

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

int beepcount = 0;

void sysbeepstop(void) {
	/* disable counter 2 */
	outb(inb_p(0x61)&0xFC, 0x61);
}

static void sysbeep(void) {
	/* enable counter 2 */
	outb_p(inb_p(0x61)|3, 0x61);
	/* set command for counter 2, 2 byte write */
	outb_p(0xB6, 0x43);
	/* send 0x637 for 750 HZ */
	outb_p(0x37, 0x42);
	outb(0x06, 0x42);
	/* 1/8 second */
	beepcount = HZ/8;	
}

static inline unsigned char new_get_fs_byte(const char * addr) {
	unsigned register char _v;
	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

void blank_screen() {
	if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		return;
}

void unblank_screen() {
	if (video_type != VIDEO_TYPE_EGAC && video_type != VIDEO_TYPE_EGAM)
		return;
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