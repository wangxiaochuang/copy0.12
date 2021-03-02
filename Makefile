CCCOLOR     = "\033[34m"
LINKCOLOR   = "\033[34;1m"
SRCCOLOR    = "\033[33m"
BINCOLOR    = "\033[37;1m"
MAKECOLOR   = "\033[32;1m"
ENDCOLOR    = "\033[0m"

QUIET_CC    = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK  = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;

AS	= $(QUIET_CC)as
CC	= $(QUIET_CC)gcc
LD	= $(QUIET_LINK)ld
CPP	= $(QUIET_CC)cpp -Iinclude
CFLAGS	= -m32 -c -Wall -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I include
LDFLAGS	= -N -Ttext 0x0 --oformat binary -nostdlib

ARCHIVES= kernel/kernel.o mm/mm.o fs/fs.o
DRIVERS = kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a
MATH    = kernel/math/math.a
LIBS	= lib/lib.a

VHD	= ../myos.vhd

.S.o:
	$(CC) -I include -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -o $@ $<

all:	Image

Image: boot/bootsect boot/setup tools/system
	dd conv=notrunc if=boot/bootsect of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/bootsect` + 511) / 512) seek=0 >/dev/null 2>&1
	dd conv=notrunc if=boot/setup of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/setup` + 511) / 512) seek=1 >/dev/null 2>&1
	dd conv=notrunc if=tools/system of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s tools/system` + 511) / 512)      seek=5 >/dev/null 2>&1

boot/head.o: boot/head.S

tools/system: boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) -e startup_32 boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system

kernel/math/math.a:
	@(cd kernel/math; make)

kernel/blk_drv/blk_drv.a:
	@(cd kernel/blk_drv; make)

kernel/chr_drv/chr_drv.a:
	@(cd kernel/chr_drv; make)

kernel/kernel.o:
	@(cd kernel; make)

mm/mm.o:
	@(cd mm; make)

fs/fs.o:
	@(cd fs; make)

lib/lib.a:
	@(cd lib; make)

boot/setup: boot/setup.S include/linux/config.h
	$(CC) -I include -c -o boot/setup.o boot/setup.S
	$(LD) $(LDFLAGS) -e 0 -o boot/setup boot/setup.o

boot/bootsect: boot/bootsect.S include/linux/config.h
	$(CC) -I include -c -o boot/bootsect.o boot/bootsect.S
	$(LD) $(LDFLAGS) -e 0 -o boot/bootsect boot/bootsect.o

bochs:
	@bochs -qf debug/bochs.cnf

swap: 
	@dd if=/dev/zero ibs=1 count=4096 | tr "\000" "\377" >../paddedFile.bin
	@dd conv=notrunc if=/dev/zero ibs=1 of=../paddedFile.bin obs=1 seek=32 count=$(shell expr 4096 - 32)
	@printf '\376' | dd conv=notrunc of=../paddedFile.bin bs=1 seek=0 count=1
	@printf 'SWAP-SPACE' | dd conv=notrunc of=../paddedFile.bin bs=1 seek=4086 count=10
	@dd conv=notrunc if=../paddedFile.bin of=../myos.vhd bs=1 seek=$(shell expr 256 \* 512) count=4096

clean:
	rm -f system.tmp tmp_make boot/bootsect boot/setup
	rm -f init/*.o tools/system boot/*.o
	(cd mm; make clean)
	(cd fs; make clean)
	(cd kernel; make clean)
	(cd lib; make clean)

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c; do echo -n "init/"; $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

### Dependencies:
init/main.o: init/main.c /usr/include/stdc-predef.h include/unistd.h \
 include/sys/stat.h include/sys/types.h include/errno.h \
 include/linux/tty.h include/termios.h include/linux/sched.h \
 include/linux/head.h include/linux/fs.h include/linux/mm.h \
 include/linux/kernel.h include/signal.h include/sys/param.h \
 include/sys/time.h include/sys/resource.h include/time.h \
 include/asm/system.h include/asm/io.h include/stddef.h include/stdarg.h \
 include/fcntl.h include/c.h
