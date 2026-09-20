#pragma once

#include <stddef.h>

template <typename T>
class vector
{
    static constexpr size_t DEFAULT_CAPACITY = 4;

    size_t capacity;
    size_t _size;
    char* data;

    void expand_capacity();

    class Iterator
    {
        T* data;
        size_t index;

    public:
        Iterator(T* data, size_t index) : data(data), index(index) {}

        T& operator*() const { return data[index]; }

        T* operator->() const {return &data[index];}

        Iterator& operator++() { ++index; return *this; }

        bool operator!=(const Iterator& other) const {
            return index != other.index;
        }
    };
public:
    vector();
    vector(const vector& other);
    vector(vector&& other);
    void push_back(const T& t);
    template <typename... Args>
    void emplace_back(Args&&... args);
    T& operator[](size_t index) const;
    void clear();
    [[nodiscard]] size_t size() const;
    Iterator begin() const;
    Iterator end() const;
};

#include "vector.hxx"