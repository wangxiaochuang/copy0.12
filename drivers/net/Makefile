NETDRV_OBJS := net.a(net_init.o)

all: net.a

net.a: $(NETDRV_OBJS)

clean:
	rm -f core *.o *.a

dep:
	$(CPP) -M *.c > .depend

tar:


# include a dependency file if one exists

ifeq (.depend,$(wildcard .depend))
include .depend
endif