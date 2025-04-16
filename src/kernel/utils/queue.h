#ifndef QUEUE_H
#define QUEUE_H

#include <kstddef.h>

template<class T, size_t N>
class queue
{
    T* data;
    size_t count = 0;
    size_t start = 0;
public:
    queue();

    ~queue();

    bool enqueue(T);

    [[nodiscard]] bool empty() const;

    [[nodiscard]] T getFirst() const;

    [[nodiscard]] bool full() const;

    T dequeue();

    [[nodiscard]] size_t getCount() const;
};

#include "queue.hxx"

#endif //QUEUE_H