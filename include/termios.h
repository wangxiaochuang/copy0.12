#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

#define NCCS            20

typedef unsigned long   tcflag_t;
typedef unsigned char   cc_t;
typedef unsigned long   speed_t;

struct termios {
	tcflag_t        c_iflag;        /* input flags */
	tcflag_t        c_oflag;        /* output flags */
	tcflag_t        c_cflag;        /* control flags */
	tcflag_t        c_lflag;        /* local flags */
	cc_t            c_cc[NCCS];     /* control chars */
	speed_t         c_ispeed;       /* input speed */
	speed_t         c_ospeed;       /* output speed */
};

#endif