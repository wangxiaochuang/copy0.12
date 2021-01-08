#define EXT_MEM_K (*(unsigned short *)0x90002)

static long memory_end = 0;             /* 机器所具有的物理内存容量 */
static long buffer_memory_end = 0;      /* 高速缓冲区末端地址 */
static long main_memory_start = 0;      /* 主内存开始位置 */

void main(void) {
    memory_end = (1 << 20) + (EXT_MEM_K << 10);
    memory_end &= 0xfffff000;
    if (memory_end > 16 * 1024 * 1024) {
        memory_end = 16 * 1024 * 1024;
    }

    if (memory_end > 12 * 1024 * 1024) {
        buffer_memory_end = 4 * 1024 * 1024;
    } else if (memory_end > 6 * 1024 * 1024) {
        buffer_memory_end = 2 * 1024 * 1024;
    } else {
        buffer_memory_end = 1 * 1024 * 1024;
    }

    main_memory_start = buffer_memory_end;
    
    mem_init(main_memory_start, memory_end);

    while(1){};
}
