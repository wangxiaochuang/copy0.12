#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <linux/segment.h>

#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")
#define nop() __asm__ __volatile__ ("nop")

#define save_flags(x) \
__asm__ __volatile__("pushfl; popl %0":"=r" (x):/* no input */:)

#define restore_flags(x) \
__asm__ __volatile__("pushl %0; popfl":/* no output */:"r" (x):)

#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ __volatile__ ("movw %%dx,%%ax\n\t" \
	"movw %2,%%dx\n\t" \
	"movl %%eax,%0\n\t" \
	"movl %%edx,%1" \
	:"=m" (*((long *) (gate_addr))), \
	 "=m" (*(1+(long *) (gate_addr))) \
	:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	 "d" ((char *) (addr)),"a" (KERNEL_CS << 16))

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

#define set_call_gate(a,addr) \
	_set_gate(a,12,3,addr)

// #define _set_tssldt_desc(n,addr,limit,type) \
__asm__ __volatile__ ("movw $" #limit ",%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	: /* no output */ \
	:"a" (addr+0xc0000000), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)
#define _set_tssldt_desc(n,addr,limit,type) \
__asm__ __volatile__ ("movw $" #limit ",(%%ebx)\n\t" \
	"movw %%ax,2(%%ebx)\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,4(%%ebx)\n\t" \
	"movb $" type ",5(%%ebx)\n\t" \
	"movb $0x00,6(%%ebx)\n\t" \
	"movb %%ah,7(%%ebx)\n\t" \
	"rorl $16,%%eax" \
	: /* no output */ \
	:"a" (addr+0xc0000000), "b" (n))

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)), 235,"0x89")
#define set_ldt_desc(n,addr,size) \
	_set_tssldt_desc(((char *) (n)),((int)(addr)),((size << 3) - 1),"0x82")

#endif