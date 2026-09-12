
#include <sys/types.h>
#include <stdint.h>

extern "C"
{
    uint8_t end;
    uint8_t _sstack;
}

static uint8_t* heap_end = nullptr;

void *_sbrk(ptrdiff_t incr)
{
    if (heap_end == nullptr)
    {
        heap_end = &end;
    }

    uint8_t* prev_heap_end = heap_end;
    uint8_t* new_heap_end = heap_end + incr;

    if (new_heap_end > &_sstack)
    {
        return (void*)-1;
    }

    heap_end = new_heap_end;

    return prev_heap_end;
}
