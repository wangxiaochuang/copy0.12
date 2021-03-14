OBJS =	fpu_entry.o

math.a: $(OBJS)
	rm -f math.a
	$(AR) rcs math.a $(OBJS)

dep:
	$(CPP) -M *.c > .depend
	# $(CPP) -D__ASSEMBLER__ -M *.S >> .depend

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif