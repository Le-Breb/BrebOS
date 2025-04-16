#pragma once

#include "queue.h"

template <class T, size_t N>
queue<T, N>::queue()
{
    data = new T[N];
}

template <class T, size_t N>
queue<T, N>::~queue()
{
    delete[] data;
}

template <class T, size_t N>
bool queue<T, N>::enqueue(T element)
{
    if (count == N)
        return false;

    data[(start + count) % N] = element;
    count++;
    return true;
}

template <class T, size_t N>
bool queue<T, N>::empty() const
{
    return count == 0;
}

template <class T, size_t N>
T queue<T, N>::getFirst() const
{
    return data[start];
}

template <class T, size_t N>
bool queue<T, N>::full() const
{
    return count == N;
}

template <class T, size_t N>
T queue<T, N>::dequeue()
{
    T el = data[start];
    start = (start + 1) % N;
    count--;

    return el;
}

template <class T, size_t N>
size_t queue<T, N>::getCount() const
{
    return count;
}

