#!Makefile

C_SOURCES = $(shell find . -name "*.c")
C_OBJECTS = $(patsubst %.c, %.o, $(C_SOURCES))
S_SOURCES = $(shell find . -name "*.S" -not -name bootsect.S -not -name setup.S)
S_OBJECTS = $(patsubst %.S, %.o, $(S_SOURCES))
OBJECTS = $(C_OBJECTS) $(S_OBJECTS)

VHD = ../myos.vhd

CC = gcc
C_FLAGS = -c -Wall -m32 -ggdb -gstabs+ -nostdinc -fno-pic -fno-builtin -fno-stack-protector -I include
LD = ld
LD_FLAGS = -N -Ttext 0x0 --oformat binary -nostdlib


all: system.bin boot/bootsect.bin boot/setup.bin

burn: boot/bootsect.bin boot/setup.bin system.bin
	dd conv=notrunc if=boot/bootsect.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/bootsect.bin` + 511) / 512) seek=0 >/dev/null 2>&1
	dd conv=notrunc if=boot/setup.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s boot/setup.bin` + 511) / 512) seek=1 >/dev/null 2>&1
	dd conv=notrunc if=system.bin of=$(VHD) bs=512 count=$(shell expr $(shell expr `stat -c %s system.bin` + 511) / 512) seek=5 >/dev/null 2>&1

	
boot/bootsect.bin: boot/bootsect.o
	$(LD) $(LD_FLAGS) -e 0 -o $@ $^

boot/setup.bin: boot/setup.o
	$(LD) $(LD_FLAGS) -e 0 -o $@ $^

system.bin: $(OBJECTS)
	$(LD) $(LD_FLAGS) -e startup_32 -o $@ $^

%.o: %.S
	$(CC) $(C_FLAGS) $< -o $@

%.o: %.c
	$(CC) $(C_FLAGS) $< -o $@

.PHONY: bochs
bochs:
	@bochs -qf debug/bochs.cnf
.PHONY: clean
clean:
	rm -rf system.bin ./boot/bootsect.bin ./boot/setup.bin $(OBJECTS) boot/*.o
