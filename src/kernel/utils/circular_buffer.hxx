#pragma once

#include "circular_buffer.h"
#include "comparison.h"
#include "kstring.h"

template <typename T>
CircularBuffer<T>::CircularBuffer(size_t capacity, mem_allocator allocator) : capacity(capacity), allocator(allocator ? allocator : (mem_allocator)malloc)
{
    data = (T*)allocator(capacity * sizeof(T));
    limit = data + capacity;
    beg = data;
}

template <typename T>
typename CircularBuffer<T>::section* CircularBuffer<T>::read(size_t n)
{
    auto* res = (section*)malloc(2 * sizeof(section));
    n = min(n, size);
    if (beg + n > limit)
    {
        auto s1_size = (size_t)(limit - beg);
        res[0] = section{beg, s1_size};
        res[1] = section{data, n - s1_size};
    }
    else
    {
        res[0] = section{beg, n};
        res[1] = section{beg + n, 0};
    }

    beg += n;
    if (beg >= limit)
        beg -= capacity;
    size -= n;

    return res;
}

template <typename T>
size_t CircularBuffer<T>::write(T* buf, size_t n)
{
    n = min(n, capacity - size);

    if (beg + size + n <= limit)
    {
        memcpy(beg + size, buf, n);
    }
    else {
        size_t s1 = limit - beg - size;
        memcpy(beg + size, buf, s1);
        size_t s2 = n - s1;
        memcpy(data, buf + s1, s2);
    }

    size += n;
    return n;
}
