OBJS = memory.o kmalloc.o swap.o vmalloc.o mmap.o

mm.o: $(OBJS)
	$(LD) -r -o mm.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif