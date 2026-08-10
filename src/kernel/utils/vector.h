#pragma once

template <typename T>
class vector
{
    static constexpr size_t DEFAULT_CAPACITY = 4;

    size_t capacity;
    size_t size;
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
    void push_back(const T& t);
    T& operator[](size_t index) const;
    void clear();
    [[nodiscard]] size_t get_size() const;
    Iterator begin() const;
    Iterator end() const;
};

#include "vector.hxx"