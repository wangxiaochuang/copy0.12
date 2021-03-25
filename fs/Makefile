SUBDIRS = minix ext ext2 msdos proc isofs nfs xiafs hpfs sysv

ifdef CONFIG_MINIX_FS
FS_SUBDIRS := $(FS_SUBDIRS) minix
endif
ifdef CONFIG_EXT_FS
FS_SUBDIRS := $(FS_SUBDIRS) ext
endif
ifdef CONFIG_EXT2_FS
FS_SUBDIRS := $(FS_SUBDIRS) ext2
endif
ifdef CONFIG_MSDOS_FS
FS_SUBDIRS := $(FS_SUBDIRS) msdos
endif
ifdef CONFIG_PROC_FS
FS_SUBDIRS := $(FS_SUBDIRS) proc
endif
ifdef CONFIG_ISO9660_FS
FS_SUBDIRS := $(FS_SUBDIRS) isofs
endif
ifdef CONFIG_NFS_FS
FS_SUBDIRS := $(FS_SUBDIRS) nfs
endif
ifdef CONFIG_XIA_FS
FS_SUBDIRS := $(FS_SUBDIRS) xiafs
endif
ifdef CONFIG_SYSV_FS
FS_SUBDIRS := $(FS_SUBDIRS) sysv
endif
ifdef CONFIG_HPFS_FS
FS_SUBDIRS := $(FS_SUBDIRS) hpfs
endif

OBJS = inode.o devices.o file_table.o buffer.o super.o open.o namei.o \
	block_dev.o fcntl.o read_write.o \
	fifo.o filesystems.o $(BINFMTS)


all: fs.o filesystems.a

fs.o: $(OBJS)
	$(LD) -r -o fs.o $(OBJS)

filesystems.a: dummy
	rm -f filesystems.a
	set -e; for i in $(FS_SUBDIRS); do \
	  test ! -d $$i || \
	    { $(MAKE) -C $$i; $(AR) rcs filesystems.a $$i/$$i.o; }; done

depend dep:
	$(CPP) -M *.c > .depend
	set -e; for i in $(SUBDIRS); do \
	  test ! -d $$i || $(MAKE) -C $$i dep; done

dummy:

ifeq (.depend,$(wildcard .depend))
include .depend
endif