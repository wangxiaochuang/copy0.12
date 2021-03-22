#ifndef _ASM_DMA_H
#define _ASM_DMA_H

#include <asm/io.h>


/* These are in kernel/dma.c: */
extern int request_dma(unsigned int dmanr);	/* reserve a DMA channel */
extern void free_dma(unsigned int dmanr);	/* release it again */

#endif