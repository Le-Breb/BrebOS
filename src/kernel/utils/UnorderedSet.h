#pragma once

#include <functional>

#include "Hash.h"
#include "kstddef.h"
#include "Status.h"

template <typename T, size_t capacity, typename hash_func = Hash<T>, typename equal_func = std::equal_to<T>>
class UnorderedSet
{
    static_assert(capacity && capacity % 2 == 0,
        "HashMap capacity must be a strictly positive power of 2");

    size_t size = 0;
    struct elem
    {
        alignas(T) char data[sizeof(T)];
        bool used;
    };
    elem elements[capacity];

public:
    Status add(const T& element);
    bool is_present(const T& t);
    void remove(const T& element);
    Optional<T*> find(const T& element);
};

#include "UnorderedSet.hxx"
