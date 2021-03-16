OBJS  = mem.o tty_io.o keyboard.o console.o defkeymap.o

all: char.a

char.a: $(OBJS)
	$(AR) rcs char.a $(OBJS)

dep:
	$(CPP) -M *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif