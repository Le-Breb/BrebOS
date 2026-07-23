#pragma once

#include "UnorderedSet.h"
#include <stdint.h>
#include "optional.h"

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template<typename K>
requires std::invocable<hash_func, const K&>
size_t UnorderedSet<T, capacity, hash_func, equal_func>::bucket(const K& key) const
{
    return hash(key) & (capacity - 1);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename K>
requires kdetail::FindCompatible<hash_func, equal_func, T, K>
size_t UnorderedSet<T, capacity, hash_func, equal_func>::get_index(const K& element) const
{
    uint32_t index = bucket(element);
    while (elements[index].used)
    {
        if (equal(elements[index].value(), element))
            return index;
        advance_index(index);
    }

    return capacity;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::advance_index(uint32_t& index)
{
    index = (index + 1) & (capacity - 1);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
UnorderedSet<T, capacity, hash_func, equal_func>::~UnorderedSet()
{
    for (auto& e : elements)
        if (e.used)
            std::destroy_at(&e.value());
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
Status UnorderedSet<T, capacity, hash_func, equal_func>::add(const T& element)
{
    if (size == capacity)
        return Status::failure();

    uint32_t index = bucket(element);
    while (elements[index].used)
    {
        if (equal(const_cast<const T&>(elements[index].value()), element))
            return Status::success(); // Element already present
        advance_index(index);
    }

    std::construct_at(elements[index].ptr(), element);
    elements[index].used = true;
    size++;

    return Status::success();
}


template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
bool UnorderedSet<T, capacity, hash_func, equal_func>::is_present(const T& t) const
{
    return get_index(t) != capacity;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::remove(const T& element)
{
    uint32_t i = get_index(element);
    if (i == capacity)
        irrecoverable_error("%s: element is not present", __PRETTY_FUNCTION__);

    std::destroy_at(&elements[i].value());
    elements[i].used = false;

    uint32_t j = (i + 1) & (capacity - 1);

    while (elements[j].used)
    {
        const uint32_t home = bucket(elements[j].value());

        // Check if element should be shifted back
        if ((home <= i && i < j) or
            (i < j && j < home) or
            (j < home && home <= i))
        {
            std::construct_at(elements[i].ptr(), std::move(elements[j].value()));
            std::destroy_at(&elements[j].value());
            elements[j].used = false;
            i = j;
        }

        j = (j + 1) & (capacity - 1);
    }

    --size;
}


template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
template <typename K>
requires kdetail::FindCompatible<hash_func, equal_func, T, K>
Optional<const T*> UnorderedSet<T, capacity, hash_func, equal_func>::find(const K& element) const
{
    uint32_t index = bucket(element);
    while (elements[index].used)
    {
        if (equal(elements[index].value(), element))
            return {elements[index].ptr()};
        advance_index(index);
    }

    return nullopt;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
[[nodiscard]]
uint32_t UnorderedSet<T, capacity, hash_func, equal_func>::get_size() const
{
    return size;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
bool UnorderedSet<T, capacity, hash_func, equal_func>::is_full() const
{
    return size == capacity;
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::Iterator UnorderedSet<T, capacity, hash_func, equal_func>::
begin()
{
    return Iterator(0, elements);
}

template <typename T, uint32_t capacity, typename hash_func, typename equal_func>
typename UnorderedSet<T, capacity, hash_func, equal_func>::Iterator UnorderedSet<T, capacity, hash_func, equal_func>::
end()
{
    return Iterator(capacity, elements);
}