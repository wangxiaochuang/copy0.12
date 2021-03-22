OBJS  = ll_rw_blk.o genhd.o floppy.o

ifdef CONFIG_BLK_DEV_HD
	OBJS := hd.o $(OBJS)
endif

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