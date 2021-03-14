OBJS  = ctype.o

lib.a: $(OBJS)
	$(AR) rcs lib.a $(OBJS)

dep:
	$(CPP) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif