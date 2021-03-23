OBJS = super.o

ext2.o: $(OBJS)
	$(LD) -r -o ext2.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif