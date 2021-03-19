OBJS = 

ifdef CONFIG_SYSVIPC
OBJS := $(OBJS) shm.o
endif

ipc.o: $(OBJS)
	$(LD) -r -o ipc.o $(OBJS)

dep:
	$(CPP) -M *.c > .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif