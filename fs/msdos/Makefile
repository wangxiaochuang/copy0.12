OBJS = inode.o

msdos.o: $(OBJS)
	$(LD) -r -o msdos.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif