#include "TmpString.h"
#include "../core/SlabAlloc.h"

TmpString::TmpString(uint size) : size(size)
{
    data = Memory::slab_allocate(size);
    data[0] = '\0';
}

TmpString::~TmpString()
{
    Memory::slab_deallocate(data, size);
}

TmpString::TmpString(const TmpString& other)
    : size(other.size)
{
    data = Memory::slab_allocate(size);
    memcpy(data, other.data, size);
}

TmpString& TmpString::operator=(const TmpString& other)
{
    if (this == &other)
        return *this;

    Memory::slab_deallocate(data, size);

    size = other.size;
    data = Memory::slab_allocate(size);
    memcpy(data, other.data, size);

    return *this;
}

TmpString::TmpString(TmpString&& other) noexcept
    : size(other.size), data(other.data)
{
    other.size = 0;
    other.data = nullptr;
}

TmpString& TmpString::operator=(TmpString&& other) noexcept
{
    if (this == &other)
        return *this;

    Memory::slab_deallocate(data, size);

    size = other.size;
    data = other.data;

    other.size = 0;
    other.data = nullptr;

    return *this;
}

char* TmpString::operator*() const
{
    return data;
}

TmpString& TmpString::concat(const TmpString& other)
{
    char* original_data = data;
    const uint original_size = size;

    size += other.size;
    char* new_data = Memory::slab_allocate(size);
    strcpy(new_data, data);
    strcat(new_data, *other);
    data = new_data;

    Memory::slab_deallocate(original_data, original_size);

    return *this;
}