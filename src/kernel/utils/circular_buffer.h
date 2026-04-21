#ifndef BREBOS_CIRCULAR_BUFFER_H
#define BREBOS_CIRCULAR_BUFFER_H

#include <kstddef.h>

template <typename T>
class CircularBuffer
{
    size_t capacity = 0;
    T* beg = nullptr;
    size_t size = 0;
    T* limit = nullptr;
public:
    struct section
    {
        T* beg;
        size_t size;
    };

    typedef void* (*mem_allocator)(uint);
    mem_allocator allocator = nullptr;
    T* data = nullptr;

    explicit CircularBuffer(size_t capacity, mem_allocator allocator = nullptr);

   section* read(size_t n);

    size_t write(T* buf, size_t n);
};

#include "circular_buffer.hxx"
#endif //BREBOS_CIRCULAR_BUFFER_H
