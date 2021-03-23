OBJS = inode.o

proc.o: $(OBJS)
	$(LD) -r -o proc.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif