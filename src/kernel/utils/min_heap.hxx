#pragma once

#include "min_heap.h"

#include "../core/fb.h"

#define ERR_EL (uint)-1

template<typename T>
uint MinHeap<T>::parent(uint i)
{
    return ((i - 1) / 2);
}

template<typename T>
uint MinHeap<T>::left(uint i) const
{
    uint l = 2 * i + 1;
    return l < count ? l : ERR_EL;
}

template<typename T>
uint MinHeap<T>::right(uint i) const
{
    uint r = 2 * i + 2;
    return r < count ? r : ERR_EL;
}

template <typename T>
MinHeap<T>::MinHeap(uint capacity) : capacity(capacity), elements(new T[capacity])
{
}

template <typename T>
MinHeap<T>::~MinHeap()
{
    delete[] elements;
}

template <typename T>
void MinHeap<T>::min_heapify(uint i)
{
    uint l = left(i);
    uint r = right(i);
    uint m = (l != ERR_EL && elements[l] < elements[i]) ? l : i;
    m = (r != ERR_EL && elements[r] < elements[m]) ? r : m;

    if (m != i)
    {
        T tmp = elements[i];
        elements[i] = elements[m];
        elements[m] = tmp;
        min_heapify(m);
    }
}

template <typename T>
void MinHeap<T>::insert(T x)
{
    if (count == capacity)
        irrecoverable_error("Heap cannot accept more elements");

    elements[count++] = x;
    uint cur = count - 1;
    uint p = parent(cur);
    while (cur != 0 && elements[p] > elements[cur])
    {
        T tmp = elements[cur];
        elements[cur] = elements[p];
        elements[p] = tmp;
        cur = p;
        p = parent(cur);
    }
}

template <typename T>
T MinHeap<T>::min() const
{
    if (count == 0)
        irrecoverable_error("empty heap");

    return elements[0];
}

template <typename T>
T MinHeap<T>::delete_min()
{
    if (count == 0)
        irrecoverable_error("empty heap");

    T min = elements[0];
    elements[0] = elements[--count];
    min_heapify(0);

    return min;
}

template <typename T>
bool MinHeap<T>::empty() const
{
    return count == 0;
}
