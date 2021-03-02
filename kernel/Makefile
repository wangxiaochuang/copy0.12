CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../include
CPP	+= -I../include

OBJS	= sched.o sys_call.o traps.o asm.o fork.o \
	  panic.o printk.o vsprintf.o sys.o exit.o \
	  signal.o mktime.o

kernel.o: $(OBJS)
	$(LD) -r -o $@ $^

clean:
	rm -f core *.o *.a tmp_make
	(cd chr_drv; make clean)
	(cd blk_drv; make clean)
	(cd math; make clean)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile
	(cd chr_drv; make dep)
	(cd blk_drv; make dep)

### Dependencies:
exit.o: exit.c /usr/include/stdc-predef.h ../include/errno.h \
 ../include/signal.h ../include/sys/types.h ../include/sys/wait.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/sys/param.h \
 ../include/sys/time.h ../include/sys/resource.h ../include/linux/tty.h \
 ../include/termios.h ../include/asm/segment.h
fork.o: fork.c /usr/include/stdc-predef.h ../include/errno.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/sys/types.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/asm/system.h
mktime.o: mktime.c /usr/include/stdc-predef.h ../include/time.h
panic.o: panic.c /usr/include/stdc-predef.h ../include/stdarg.h \
 ../include/stddef.h ../include/linux/kernel.h
printk.o: printk.c /usr/include/stdc-predef.h ../include/stdarg.h \
 ../include/stddef.h ../include/linux/kernel.h
sched.o: sched.c /usr/include/stdc-predef.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/linux/sys.h ../include/asm/system.h ../include/asm/io.h
signal.o: signal.c /usr/include/stdc-predef.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/segment.h ../include/errno.h
sys.o: sys.c /usr/include/stdc-predef.h ../include/errno.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/sys/types.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h ../include/linux/tty.h ../include/termios.h \
 ../include/linux/config.h ../include/asm/segment.h \
 ../include/sys/times.h ../include/sys/utsname.h ../include/string.h
traps.o: traps.c /usr/include/stdc-predef.h ../include/linux/head.h \
 ../include/linux/sched.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h \
 ../include/asm/system.h ../include/asm/io.h
vsprintf.o: vsprintf.c /usr/include/stdc-predef.h ../include/stdarg.h \
 ../include/string.h
