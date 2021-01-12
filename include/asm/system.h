#define sti() __asm__ ("sti"::)			/* 开中断 */
#define cli() __asm__ ("cli"::)			/* 关中断 */
#define nop() __asm__ ("nop"::)			/* 空操作 */

#define iret() __asm__ ("iret"::)		/* 中断返回 */

#define _set_gate(gate_addr, type, dpl, addr)	            \
    __asm__ ("movw %%dx,%%ax\n\t"                           \
            "movw %0,%%dx\n\t"                              \
            "movl %%eax,%1\n\t"                             \
            "movl %%edx,%2"                                 \
            :                                               \
            : "i" ((short) (0x8000+(dpl<<13)+(type<8))),    \
            "o" (*((char *) (gate_addr))),                  \
            "o" (*(4+(char *) (gate_addr))),                \
            "d" ((char *) (addr)),"a" (0x00080000))
#define set_trap_gate(n, addr)  _set_gate(&idt[n], 15, 0, addr)
#define set_system_gate(n, addr)    _set_gate(&idt[n], 15, 3, addr)

/**
 * 在全局表中设置任务状态段/局部表描述符
 * 状态段局部表段的长度均被设置成104字节。%0 - eax(地址addr)；%1 - (描述符项n的地址); %2 - (描述
 * 符项n的地址偏移2处)；%3 - (描述符项n的地址偏移4处); %4 - (描述符项n的地址偏移5处);%5 - (描述
 * 符项n的地址偏移6处);%6 - (描述符项n的地址偏移7处);
 * @param[in]	n		在全局表中描述符项n所对应的地址
 * @param[in]	addr	状态段/局部表所在内存的基地址
 * @param[in]	type	描述符中的标志类型字节
 */
#define _set_tssldt_desc(n,addr,type)								\
	__asm__ (														\
	"movw $104,%1\n\t"												\
	"movw %%ax,%2\n\t"												\
	"rorl $16,%%eax\n\t"											\
	"movb %%al,%3\n\t"												\
	"movb $" type ",%4\n\t"											\
	"movb $0x00,%5\n\t"												\
	"movb %%ah,%6\n\t"												\
	"rorl $16,%%eax"												\
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)),			\
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7))						\
	)

#define set_tss_desc(n,addr)	_set_tssldt_desc(((char *) (n)), addr, "0x89")

#define set_ldt_desc(n, addr)	_set_tssldt_desc(((char *) (n)), addr, "0x82")