#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

static inline unsigned char get_user_byte(const char * addr) {
	register unsigned char _v;

	__asm__ ("movb %%fs:%1,%0":"=q" (_v):"m" (*addr));
	return _v;
}

#define get_fs_byte(addr) get_user_byte((char *)(addr))

#endif