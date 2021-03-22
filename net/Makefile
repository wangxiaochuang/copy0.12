OBJS = socket.o

all: net.o

net.o: $(OBJS)
	$(LD) -r -o net.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend
	# set -e; for i in $(SUBDIRS); do $(MAKE) -C $$i dep; done

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
	