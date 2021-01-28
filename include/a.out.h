#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

struct exec {
  unsigned long a_magic;/* Use macros N_MAGIC, etc for access */
  						/* 执行文件魔数 */
  unsigned long a_text;		/* length of text, in bytes */
  						/* 代码长度 */
  unsigned long a_data;		/* length of data, in bytes */
  						/* 数据长度 */
  unsigned long a_bss;		/* length of uninitialized data area for file, in bytes */
  						/* 未初始化数据区的长度 */
  unsigned long a_syms;		/* length of symbol table data in file, in bytes */
  						/* 符号表的长度 */
  unsigned long a_entry;		/* start address */
  						/* 执行开始地址 */
  unsigned long a_trsize;	/* length of relocation info for text, in bytes */
  						/* 代码重定位信息的长度 */
  unsigned long a_drsize;	/* length of relocation info for data, in bytes */
  						/* 数据重定位信息的长度 */
};

#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

/* OMAGIC和ZMAGIC的主要区别在于它们对各个部分的存储分配方式上。虽然该结构的总长度只有32B，但
 是对于一个ZMAGIC类型的执行文件来说，其文件开始部分却需要专门留出1KB的空间给头结构使用。除被头
 结构占用的32B以外，其余部分均为0。从1024字节之后才开始放置程序的正文段和数据段等信息。而对于一
 个OMAGIC类型的.o模块文件来说，文件开始部分的32字节头结构后面紧接着就是代码区和数据区。 */
#ifndef OMAGIC

/* Code indicating object file or impure executable. */
/* OMAGIC(Old Magic)指明文件是目标文件或者不纯的可执行文件 */
#define OMAGIC 0407

/* Code indicating pure executable. */
/* 文件为纯粹的可执行文件  */ 
#define NMAGIC 0410

/* Code indicating demand-paged executable. */
/* 文件为需求分页处理（demand-paged，即需求加载）的可执行文件 */
#define ZMAGIC 0413
#endif /* not OMAGIC */

#ifndef N_BADMAG
#define N_BADMAG(x)					\
	(N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC && N_MAGIC(x) != ZMAGIC)
#endif

#define _N_BADMAG(x)					\
	(N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC && N_MAGIC(x) != ZMAGIC)

#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec))

#ifndef N_TXTOFF
#define N_TXTOFF(x) \
 (N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof (struct exec) : sizeof (struct exec))
#endif

#ifndef N_DATOFF
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

#ifndef N_TRELOFF
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

#ifndef N_DRELOFF
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

#ifndef N_SYMOFF
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

#ifndef N_STROFF
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif

/* Address of text segment in memory after it is loaded.  */
#ifndef N_TXTADDR
#define N_TXTADDR(x) 0
#endif

#define PAGE_SIZE 4096
#define SEGMENT_SIZE 1024

#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1))

#define _N_TXTENDADDR(x) (N_TXTADDR(x)+(x).a_text)

#ifndef N_DATADDR
#define N_DATADDR(x) \
    (N_MAGIC(x)==OMAGIC ? (_N_TXTENDADDR(x)) : (_N_SEGMENT_ROUND (_N_TXTENDADDR(x))))
#endif

/* Address of bss segment in memory after it is loaded.  */
#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data)
#endif

#endif