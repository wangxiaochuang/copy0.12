CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../include
CPP     += -I../include

OBJS	= memory.o swap.o page.o

mm.o: $(OBJS)
	$(LD) -r -o $@ $^

clean:
	rm -f core *.o *.a tmp_make

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
memory.o: memory.c /usr/include/stdc-predef.h ../include/linux/sched.h \
 ../include/linux/head.h ../include/linux/fs.h ../include/sys/types.h \
 ../include/linux/mm.h ../include/linux/kernel.h ../include/signal.h \
 ../include/sys/param.h ../include/sys/time.h ../include/sys/resource.h
swap.o: swap.c /usr/include/stdc-predef.h ../include/string.h \
 ../include/linux/sched.h ../include/linux/head.h ../include/linux/fs.h \
 ../include/sys/types.h ../include/linux/mm.h ../include/linux/kernel.h \
 ../include/signal.h ../include/sys/param.h ../include/sys/time.h \
 ../include/sys/resource.h
