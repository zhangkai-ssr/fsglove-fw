/*
 * sysmem.c — newlib 堆管理 _sbrk（裸机）
 * 堆从链接脚本的 _end 开始增长，上限到栈预留区。
 */
#include <stdint.h>
#include <errno.h>

extern uint8_t _end;          /* 链接脚本提供：堆起点 */
extern uint8_t _estack;       /* 栈顶 */
extern uint32_t _Min_Stack_Size;

void *_sbrk(ptrdiff_t incr)
{
    static uint8_t *heap_end = 0;
    uint8_t *prev_heap_end;
    const uint8_t *stack_limit = (uint8_t *)((uint32_t)&_estack - (uint32_t)&_Min_Stack_Size);

    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev_heap_end = heap_end;

    if (heap_end + incr > stack_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return (void *)prev_heap_end;
}
