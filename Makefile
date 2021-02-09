#!Makefile

C_INCS = $(shell find . -name "*.h")
C_SOURCES = $(shell find . -name "*.c" -not -name makeswap.c)
C_OBJECTS = $(patsubst %.c, %.o, $(C_SOURCES))
S_SOURCES = $(shell find . -name "*.S" -not -name bootsect.S -not -name setup.S)
S_OBJECTS = $(patsubst %.S, %.o, $(S_SOURCES))
OBJECTS = $(S_OBJECTS) $(C_OBJECTS)

VHD = ../myos.vhd

CC = gcc
#C_FLAGS = -c -m32 -O0 -fno-asynchronous-unwind-tables -ffreestanding -mpreferred-stack-boundary=2  -I include
C_FLAGS = -c -m32 -lasan -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector  -I include
LD = ld
LD_FLAGS = -N -Ttext 0x0 --oformat binary -nostdlib


all: system.bin boot/bootsect.bin boot/setup.bin

burn: boot/bootsect.bin boot/setup.bin system.bin
	dd conv=notrunc if=boot/bootsect.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/bootsect.bin` + 511) / 512) seek=0 >/dev/null 2>&1
	dd conv=notrunc if=boot/setup.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/setup.bin` + 511) / 512) seek=1 >/dev/null 2>&1
	dd conv=notrunc if=system.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s system.bin` + 511) / 512) seek=5 >/dev/null 2>&1

swap: system.bin
	@dd if=/dev/zero ibs=1 count=4096 | tr "\000" "\377" >../paddedFile.bin
	@dd conv=notrunc if=/dev/zero ibs=1 of=../paddedFile.bin obs=1 seek=32 count=$(shell expr 4096 - 32)
	@printf '\376' | dd conv=notrunc of=../paddedFile.bin bs=1 seek=0 count=1
	@printf 'SWAP-SPACE' | dd conv=notrunc of=../paddedFile.bin bs=1 seek=4086 count=10
	@dd conv=notrunc if=../paddedFile.bin of=../myos.vhd bs=1 seek=$(shell expr 256 \* 512) count=4096
	
boot/bootsect.bin: boot/bootsect.o
	$(LD) $(LD_FLAGS) -e 0 -o $@ $^

boot/setup.bin: boot/setup.o
	$(LD) $(LD_FLAGS) -e 0 -o $@ $^

system.bin: $(OBJECTS)
	$(LD) $(LD_FLAGS) -e startup_32 -o $@ $^

%.o: %.S
	$(CC) -I include -c -o $@ $<

%.o: %.c $(C_INCS)
	$(CC) $(C_FLAGS) -o $@ $<

.PHONY: bochs
bochs:
	@bochs -qf debug/bochs.cnf
.PHONY: clean
clean:
	rm -rf system.bin ./boot/bootsect.bin ./boot/setup.bin $(OBJECTS) boot/*.o
