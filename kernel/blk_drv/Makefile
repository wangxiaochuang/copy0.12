CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../../include
CPP     += -I../../include

OBJS	= ll_rw_blk.o floppy.o hd.o ramdisk.o

blk_drv.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f core *.o *.a tmp_make

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
floppy.o: floppy.c /usr/include/stdc-predef.h
hd.o: hd.c /usr/include/stdc-predef.h ../../include/linux/sched.h \
 ../../include/linux/head.h ../../include/linux/fs.h \
 ../../include/sys/types.h ../../include/linux/mm.h \
 ../../include/linux/kernel.h ../../include/signal.h \
 ../../include/sys/param.h ../../include/sys/time.h \
 ../../include/sys/resource.h ../../include/linux/hdreg.h \
 ../../include/asm/system.h ../../include/asm/io.h blk.h
ll_rw_blk.o: ll_rw_blk.c /usr/include/stdc-predef.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/asm/system.h blk.h
ramdisk.o: ramdisk.c /usr/include/stdc-predef.h \
 ../../include/linux/sched.h ../../include/linux/head.h \
 ../../include/linux/fs.h ../../include/sys/types.h \
 ../../include/linux/mm.h ../../include/linux/kernel.h \
 ../../include/signal.h ../../include/sys/param.h \
 ../../include/sys/time.h ../../include/sys/resource.h \
 ../../include/asm/system.h blk.h
