#pragma once

#include "Stack.h"

template <typename T>
Stack<T>::Stack(size_t capacity) : capacity(capacity)
{
    data = new T*[capacity];
}

template <typename T>
Stack<T>::~Stack()
{
    delete[] data;
}

template <typename T>
bool Stack<T>::push(const T* t)
{
    if (size == capacity)
        return false;

    data[size++] = new T(*t);

    return true;
}

template <typename T>
bool Stack<T>::push(const T& t)
{
    if (size == capacity)
        return false;

    data[size++] = new T(t);

    return true;
}

template <typename T>
T* Stack<T>::pop()
{
    if (!size)
        return nullptr;

    return data[--size];
}

template <typename T>
T* Stack<T>::peek()
{
    if (!size)
        return nullptr;
    return data[size - 1];
}

template <typename T>
size_t Stack<T>::get_size() const
{
    return size;
}
