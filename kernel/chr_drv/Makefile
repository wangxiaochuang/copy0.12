CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../../include
CPP     += -I../../include

OBJS	= tty_io.o console.o keyboard.o serial.o rs_io.o \
	  tty_ioctl.o pty.o

chr_drv.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f core *.o *.a tmp_make

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
console.o: console.c /usr/include/stdc-predef.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/linux/tty.h ../../include/termios.h ../../include/asm/io.h \
 ../../include/asm/system.h
pty.o: pty.c /usr/include/stdc-predef.h ../../include/linux/tty.h \
 ../../include/termios.h ../../include/sys/types.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/asm/system.h \
 ../../include/asm/io.h
serial.o: serial.c /usr/include/stdc-predef.h ../../include/linux/tty.h \
 ../../include/termios.h ../../include/sys/types.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/asm/system.h \
 ../../include/asm/io.h
tty_io.o: tty_io.c /usr/include/stdc-predef.h ../../include/ctype.h \
 ../../include/errno.h ../../include/signal.h ../../include/sys/types.h \
 ../../include/unistd.h ../../include/sys/stat.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/linux/tty.h ../../include/termios.h \
 ../../include/asm/segment.h ../../include/asm/system.h
tty_ioctl.o: tty_ioctl.c /usr/include/stdc-predef.h ../../include/errno.h \
 ../../include/termios.h ../../include/sys/types.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/linux/tty.h \
 ../../include/asm/io.h ../../include/asm/segment.h \
 ../../include/asm/system.h
