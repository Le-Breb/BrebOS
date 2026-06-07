#pragma once

#include <unistd.h>

template <typename T>
class Stack
{
    T** data = nullptr;
    size_t size = 0;
    size_t capacity;

public:

    explicit Stack(size_t capacity);

    ~Stack();

    bool push(const T* t);
    bool push(const T& t);

    T* pop();
    [[nodiscard]]
    T* peek();

    [[nodiscard]]
    size_t get_size() const;
};

#include "Stack.hxx"