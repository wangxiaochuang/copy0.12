OBJS = inode.o

nfs.o: $(OBJS)
	$(LD) -r -o nfs.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif