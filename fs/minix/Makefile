OBJS = inode.o

minix.o: $(OBJS)
	$(LD) -r -o minix.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif