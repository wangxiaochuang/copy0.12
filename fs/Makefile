SUBDIRS = 

OBJS = super.o devices.o

all: fs.o

fs.o: $(OBJS)
	$(LD) -r -o fs.o $(OBJS)

depend dep:
	$(CPP) -M *.c > .depend
	set -e; for i in $(SUBDIRS); do \
	  test ! -d $$i || $(MAKE) -C $$i dep; done

dummy:

ifeq (.depend,$(wildcard .depend))
include .depend
endif