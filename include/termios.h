#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

#include <sys/types.h>

#define TTY_BUF_SIZE 1024

#define NCCS 17
struct termios {
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_line;			/* line discipline */
	cc_t c_cc[NCCS];		/* control characters */
};

#endif