OBJS  = ll_rw_blk.o hd.o

all: block.a

block.a: $(OBJS)
	rm -f block.a
	$(AR) rcs block.a $(OBJS)

dep:
	$(CPP) -M *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif