#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

struct exec {
  unsigned long a_magic;/* Use macros N_MAGIC, etc for access */
  						/* 执行文件魔数 */
  unsigned a_text;		/* length of text, in bytes */
  						/* 代码长度 */
  unsigned a_data;		/* length of data, in bytes */
  						/* 数据长度 */
  unsigned a_bss;		/* length of uninitialized data area for file, in bytes */
  						/* 未初始化数据区的长度 */
  unsigned a_syms;		/* length of symbol table data in file, in bytes */
  						/* 符号表的长度 */
  unsigned a_entry;		/* start address */
  						/* 执行开始地址 */
  unsigned a_trsize;	/* length of relocation info for text, in bytes */
  						/* 代码重定位信息的长度 */
  unsigned a_drsize;	/* length of relocation info for data, in bytes */
  						/* 数据重定位信息的长度 */
};

#endif