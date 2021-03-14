OBJS = sched.o sys_call.o traps.o printk.o vsprintf.o exit.o irq.o signal.o sys.o panic.o

all: kernel.o

kernel.o: $(OBJS)
	$(LD) -r -o kernel.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

dummy:

ifeq (.depend, $(wildcard .depend))
include .depend
endif