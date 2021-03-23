
VERSION = 1
PATCHLEVEL = 0
ALPHA =

VHD	= ../myfd.img

all:	Version zImage

.EXPORT_ALL_VARIABLES:

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)

ifeq (.config, $(wildcard .config))
	include .config
	ifeq (.depend, $(wildcard .depend))
		include .depend
	else
		CONFIGURATION = depend
	endif
else
	CONFIGURATION = config
endif

ifdef CONFIGURATION
	CONFIGURE = dummy
endif

ROOT_DEV = CURRENT

SVGA_MODE =	-DSVGA_MODE=NORMAL_VGA

CFLAGS	= -m32 -c -Wall -lasan -ggdb -gstabs+ -nostdinc -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-variable -fno-pic -fno-builtin -fno-stack-protector

AS	= as
LD	= ld
HOSTCC = gcc
CC	= gcc -D__KERNEL__
MAKE = make
CPP	= $(CC) -E
AR = ar
STRIP = strip
LDFLAGS	= -N --oformat binary -nostdlib

ARCHIVES	= kernel/kernel.o fs/fs.o mm/mm.o ipc/ipc.o net/net.o
FILESYSTEMS	= fs/filesystems.a
DRIVERS = drivers/block/block.a \
		 drivers/char/char.a \
 		 drivers/net/net.a
# 		 ibcs/ibcs.o
LIBS	= lib/lib.a
SUBDIRS	= kernel fs lib mm drivers ipc net #ibcs

C_INCLUDE_PATH	=$(shell pwd)/include

ifdef CONFIG_SCSI
DRIVERS := $(DRIVERS) drivers/scsi/scsi.a
endif

ifdef CONFIG_SOUND
DRIVERS := $(DRIVERS) drivers/sound/sound.a
endif

ifdef CONFIG_MATH_EMULATION
DRIVERS := $(DRIVERS) drivers/FPU-emu/math.a
endif

.S.o:
	$(CC) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) -o $@ $<

Version: dummy
	rm -f tools/version.h

config:
	$(CONFIG_SHELL) Configure $(OPTS) < config.in
	@if grep -s '^CONFIG_SOUND' .tmpconfig; then \
		$(MAKE) -C driver/sound config; \
		else : ; fi
	mv .tmpconfig .config

linuxsubdirs: dummy
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i; done

tools/./version.h: tools/version.h

tools/version.h: $(CONFIGURE) Makefile
	@sh makever.sh
	@echo \#define UTS_RELEASE \"$(VERSION).$(PATCHLEVEL)$(ALPHA)\" > tools/version.h
	@echo \#define UTS_VERSION \"\#`cat .version` `date`\" >> tools/version.h
	@echo \#define LINUX_COMPILE_TIME \"`date +%T`\" >> tools/version.h
	@echo \#define LINUX_COMPILE_BY \"`whoami`\" >> tools/version.h
	@echo \#define LINUX_COMPILE_HOST \"`hostname`\" >> tools/version.h
	@echo \#define LINUX_COMPILE_DOMAIN \"`domainname`\" >> tools/version.h

tools/version.o: tools/version.c tools/version.h

boot/bootsect: boot/bootsect.S include/linux/config.h
	$(CC) -I include -c -o boot/bootsect.o boot/bootsect.S
	$(LD) $(LDFLAGS) -Ttext 0x0 -e 0 -o boot/bootsect boot/bootsect.o

tools/system: boot/head.o init/main.o tools/version.o linuxsubdirs
	$(LD) $(LDFLAGS) -Ttext 0x1000 -e startup_32 boot/head.o init/main.o tools/version.o \
	$(ARCHIVES) \
	$(FILESYSTEMS) \
	$(DRIVERS) \
	$(LIBS) \
	-o tools/system

boot/setup: boot/setup.S include/linux/config.h
	$(CC) -I include -c -o boot/setup.o boot/setup.S
	$(LD) $(LDFLAGS) -Ttext 0x0 -e 0 -o boot/setup boot/setup.o

zImage: $(CONFIGURE) boot/bootsect boot/setup tools/system

zdisk: zImage
	dd conv=notrunc if=boot/bootsect of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/bootsect` + 511) / 512) seek=0 >/dev/null 2>&1
	dd conv=notrunc if=boot/setup of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/setup` + 511) / 512) seek=1 >/dev/null 2>&1
	dd conv=notrunc if=tools/system of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s tools/system` + 511) / 512) seek=5 >/dev/null 2>&1

fs: dummy
	$(MAKE) linuxsubdirs SUBDIRS=fs

lib: dummy
	$(MAKE) linuxsubdirs SUBDIRS=lib

mm: dummy
	$(MAKE) linuxsubdirs SUBDIRS=mm

ipc: dummy
	$(MAKE) linuxsubdirs SUBDIRS=ipc

kernel: dummy
	$(MAKE) linuxsubdirs SUBDIRS=kernel

drivers: dummy
	$(MAKE) linuxsubdirs SUBDIRS=drivers

net: dummy
	$(MAKE) linuxsubdirs SUBDIRS=net

bochs:
	@bochs -qf debug/bochs.cnf

clean:
	rm -f kernel/ksyms.lst
	rm -f core `find . -name '*.[oas]' -print`
	rm -f core `find . -name 'core' -print`
	rm -f zImage zSystem.map tools/zSystem tools/system
	rm -f Image System.map boot/bootsect boot/setup
	rm -f zBoot/zSystem zBoot/xtract zBoot/piggyback
	rm -f .tmp* drivers/sound/configure
	rm -f init/*.o tools/build boot/*.o tools/*.o

depend dep:
	touch tools/version.h
	for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done > .tmpdepend
	for i in tools/*.c;do echo -n "tools/";$(CPP) -M $$i;done >> .tmpdepend
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i dep; done
	rm -f tools/version.h
	mv .tmpdepend .depend


ifdef CONFIGURATION
..$(CONFIGURATION):
	@echo
	@echo "You have a bad or nonexistent" .$(CONFIGURATION) ": running 'make" $(CONFIGURATION)"'"
	@echo
	$(MAKE) $(CONFIGURATION)
	@echo
	@echo "Successful. Try re-making (ignore the error that follows)"
	@echo
	exit 1

dummy: ..$(CONFIGURATION)

else

dummy:

endif