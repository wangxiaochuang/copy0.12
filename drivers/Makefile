SUBDIRS = char block net
ifdef CONFIG_MATH_EMULATION
SUBDIRS := $(SUBDIRS) FPU-emu
endif

all: driversubdirs

driversubdirs: dummy
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i; done

dep:
	set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i dep; done

dummy:

ifeq (.depend,$(wildcard .depend))
include .depend
endif