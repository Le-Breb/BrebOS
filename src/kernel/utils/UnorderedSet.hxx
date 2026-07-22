#include <stdint.h>

#include "Hash.h"
#include "UnorderedSet.h"

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

template <typename T, size_t capacity, typename hash_func, typename equal_func>
Status UnorderedSet<T, capacity, hash_func, equal_func>::add(const T& element)
{
    if (size == capacity)
        return Status::failure();

    uint32_t index = hash_func(element) & (capacity - 1);
    while (elements[index].used)
    {
        if (equal_func(elements[index].data))
            return Status::success(); // Element already present
        index = (index + 1) & (capacity - 1);
    }

    *reinterpret_cast<T*>(&elements[index].data) = element;
    elements[index].used = true;
    size++;

    return Status::success();
}


template <typename T, size_t capacity, typename hash_func, typename equal_func>
bool UnorderedSet<T, capacity, hash_func, equal_func>::is_present(const T& t)
{
    uint32_t index = hash_func(t) & (capacity - 1);
    while (elements[index].used)
    {
        if (equal_func(reinterpret_cast<T&>(elements[index].data)))
            return true;
        index = (index + 1) & (capacity - 1);
    }

    return false;
}

template <typename T, size_t capacity, typename hash_func, typename equal_func>
void UnorderedSet<T, capacity, hash_func, equal_func>::remove(const T& element)
{
    uint32_t index = hash_func(element) & (capacity - 1);
    while (elements[index].used)
    {
        if (equal_func(elements[index].data, element))
        {
            reinterpret_cast<T&>(elements[index].data).~T();
            elements[index].used = false;
            return;
        }
        index = (index + 1) & (capacity - 1);
    }

    irrecoverable_error("%s: element is not present", __PRETTY_FUNCTION__);
}

template <typename T, size_t capacity, typename hash_func, typename equal_func>
Optional<T*> UnorderedSet<T, capacity, hash_func, equal_func>::find(const T& element)
{
    uint32_t index = hash_func(element) & (capacity - 1);
    while (elements[index].used)
    {
        if (equal_func(elements[index].data, element))
        {
            return {reinterpret_cast<T*>(&element[index].data)};
        }
        index = (index + 1) & (capacity - 1);
    }

    return nullopt;
}
