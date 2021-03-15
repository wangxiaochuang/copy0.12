OBJS = memory.o kmalloc.o

mm.o: $(OBJS)
	$(LD) -r -o mm.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif