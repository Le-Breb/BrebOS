#include <stdint.h>

#include "Hash.h"
#include "HashMap.h"

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

template <typename T, size_t capacity>
requires std::equality_comparable<T>
Status HashMap<T, capacity>::add(const T& element)
{
    if (size == capacity)
        return Status::failure();

    uint32_t index = Hash<T>{}(element) & (capacity - 1);
    while (elements[index].used)
        index = (index + 1) & (capacity - 1);

    *reinterpret_cast<T*>(&elements[index].data) = element;
    elements[index].used = true;
    size++;

    return Status::success();
}


template <typename T, size_t capacity>
requires std::equality_comparable<T>
bool HashMap<T, capacity>::is_present(const T& t)
{
    uint32_t index = Hash<T>{}(t) & (capacity - 1);
    while (elements[index].used)
    {
        if (reinterpret_cast<T&>(elements[index].data) == t)
            return true;
        index = (index + 1) & (capacity - 1);
    }

    return false;
}

template <typename T, size_t capacity>
requires std::equality_comparable<T>
void HashMap<T, capacity>::remove(const T& element)
{
    uint32_t index = Hash<T>{}(element) & (capacity - 1);
    while (elements[index].used)
    {
        if (reinterpret_cast<T&>(elements[index].data) == element)
        {
            reinterpret_cast<T&>(elements[index].data).~T();
            elements[index].used = false;
            return;
        }
        index = (index + 1) & (capacity - 1);
    }

    irrecoverable_error("%s: element is not present", __PRETTY_FUNCTION__);
}
