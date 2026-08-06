#pragma once

#include "vector.h"

extern "C" void* realloc(void* ptr, size_t size);

[[noreturn]]
extern int irrecoverable_error(const char* format, ...);

template <typename T>
void vector<T>::expand_capacity()
{
    capacity *= 2;
    const auto new_data = realloc(data, capacity * sizeof(T)); // Todo: make allocation aligned to alignof(T)
    if (!new_data)
        irrecoverable_error("%s: realloc failed", __PRETTY_FUNCTION__);
    data = static_cast<char*>(new_data);
}

template <typename T>
vector<T>::vector() : capacity(DEFAULT_CAPACITY), size(0), data(new char[capacity * sizeof(T)])
{
}

template <typename T>
void vector<T>::push_back(const T& t)
{
    if (size == capacity)
        expand_capacity();

    new (data + size * sizeof(T)) T(t);
    ++size;
}

template <typename T>
T& vector<T>::operator[](size_t index)
{
    if (index >= size)
        irrecoverable_error("%s: index out of bounds", __PRETTY_FUNCTION__);

    return *reinterpret_cast<T*>(&data[index * sizeof(T)]);
}

template <typename T>
void vector<T>::clear()
{
    for (size_t i = 0; i < size; ++i)
        reinterpret_cast<T*>(&data[i * sizeof(T)])->~T();
    size = 0;

    if (capacity > DEFAULT_CAPACITY)
    {
        delete[] data;
        capacity = DEFAULT_CAPACITY;
        data = new char[capacity * sizeof(T)];
    }
}

template <typename T>
size_t vector<T>::get_size() const
{
    return size;
}

template <typename T>
vector<T>::Iterator vector<T>::begin() const
{
    if (size)
        return Iterator(reinterpret_cast<T*>(data), 0);
    return end();
}

template <typename T>
vector<T>::Iterator vector<T>::end() const
{
    return Iterator(reinterpret_cast<T*>(data), size);
}
