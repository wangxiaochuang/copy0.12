CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../../include

OBJS	= math_emulate.o

math.a: $(OBJS)
	$(AR) rcs math.a $(OBJS)

clean:
	rm -f core *.o *.a tmp_make

deps:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
