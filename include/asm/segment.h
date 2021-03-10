static inline unsigned char get_fs_byte(const char * addr)
{
	unsigned register char _v;

	__asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline unsigned short get_fs_word(const unsigned short *addr) {
	unsigned short _v;

	__asm__ ("movw %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline unsigned long get_fs_long(const unsigned long *addr)
{
	unsigned long _v;

	__asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr));
	return _v;
}

static inline void put_fs_byte(char val, char *addr) {
	__asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_word(short val,short * addr)
{
	__asm__ ("movw %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
	__asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void memcpy_tofs(void *to, void *from, unsigned long n) {
	__asm__("cld\n\t"
		"push %%es\n\t"
		"push %%fs\n\t"
		"pop %%es\n\t"			// es变成fs的值，因为movs*指令的目的操作数是 es:edi
		"testb $1, %%cl\n\t"	// 位0是1，就移动一个字节
		"je 1f\n\t"
		"movsb\n"
		"1:\ttestb $2, %%cl\n\t"
		"je 2f\n\t"				// 位1是1，就移动两个字节
		"movsw\n"
		"2:\tshrl $2, %%ecx\n\t"	// cl右移2位，总体目的是要按双字进行移动
		"rep; movsl\n\t"
		"pop %%es"
		::"c" (n), "D" ((long) to), "S" ((long) from));
}

static inline void memcpy_fromfs(void * to, void * from, unsigned long n) {
	__asm__("cld\n\t"
		"testb $1, %%cl\n\t"
		"je 1f\n\t"
		"fs; movsb\n"
		"1:\ttestb $2, %%cl\n\t"
		"je 2f\n\t"
		"fs; movsw\n"
		"2:\tshrl $2, %%ecx\n\t"
		"rep ; fs ; movsl"
		::"c" (n), "D" ((long) to), "S" ((long) from));
	)
}

/**
 * 取fs段寄存器值(选择符)
 * @retval		fs段寄存器值
 */
static inline unsigned long get_fs() 
{
	unsigned short _v;
	__asm__("mov %%fs,%%ax":"=a" (_v):);
	return _v;
}

/**
 * 取ds段寄存器值
 * @retval		ds段寄存器值
 */
static inline unsigned long get_ds() 
{
	unsigned short _v;
	__asm__("mov %%ds,%%ax":"=a" (_v):);
	return _v;
}


/**
 * 设置fs段寄存器
 * @param[in]	val		段值(选择符)
 */
static inline void set_fs(unsigned long val)
{
	__asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}