CFLAGS	= -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I ../include
CPP     += -I../include

OBJS	= ctype.o _exit.o open.o close.o errno.o write.o dup.o setsid.o \
	  execve.o wait.o string.o malloc.o log_print.o

lib.a: $(OBJS)
	$(AR) rcs $@ $^

clean:
	rm -f core *.o *.a tmp_make

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in *.c; do $(CPP) -M $$i; done) >> tmp_make
	cp tmp_make Makefile

### Dependencies:
