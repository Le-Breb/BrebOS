#pragma once

#include "ELFTools.h"

#include "kstring.h"

typedef unsigned int uint;

namespace ELFTools
{
inline unsigned long hash(const unsigned char* name)
{
    unsigned long h = 0, g;
    while (*name)
    {
        h = (h << 4) + *name++;
        if ((g = (h & 0xf0000000)))
            h ^= g >> 24 ;
        h &= ~g ;
    }

    return h ;
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>::Lptr(ptr runtime_ptr, Memory::AddressSpaceBridge* address_space_bridge)
    : p_runtime_ptr(runtime_ptr), address_space_bridge(address_space_bridge)
{
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>& Lptr<ptr_underlying_type>::operator=(Elf32_Addr runtime_addr)
{
    p_runtime_ptr = reinterpret_cast<ptr>(runtime_addr);
    return *this;
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>& Lptr<ptr_underlying_type>::operator=(ptr runtime_ptr)
{
    p_runtime_ptr = runtime_ptr;
    return *this;
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type> Lptr<ptr_underlying_type>::operator+(int n) const
{
    return Lptr{p_runtime_ptr + n, address_space_bridge};
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type> Lptr<ptr_underlying_type>::operator+(unsigned int n) const
{
    return {p_runtime_ptr + n, address_space_bridge};
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>& Lptr<ptr_underlying_type>::operator++()
{
    ++p_runtime_ptr;
    return *this;
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>& Lptr<ptr_underlying_type>::operator--()
{
    --p_runtime_ptr;
    return *this;
}

template <typename ptr_underlying_type>
void Lptr<ptr_underlying_type>::operator-=(int n)
{
    p_runtime_ptr -= n;
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type> Lptr<ptr_underlying_type>::operator-(int n) const
{
    return {p_runtime_ptr - n, address_space_bridge};
}

template <typename ptr_underlying_type>
size_t Lptr<ptr_underlying_type>::operator-(const Lptr& other) const
{
    return p_runtime_ptr - other.p_runtime_ptr;
}

template <typename ptr_underlying_type>
void Lptr<ptr_underlying_type>::memset(int c, size_t n) const
{
    const auto addr = reinterpret_cast<uintptr_t>(p_runtime_ptr);
    const uintptr_t map_addr = address_space_bridge->convert(addr, n);

    if (map_addr == Memory::AddressSpaceBridge::CONVERSION_ERROR)
        irrecoverable_error("%s: couldn't convert address %x", __PRETTY_FUNCTION__, addr);

    ::memset((void*)map_addr, c, n);
}

template <typename ptr_underlying_type>
void Lptr<ptr_underlying_type>::memcpy(const void* src, size_t n) const
{
    const auto addr = reinterpret_cast<uintptr_t>(p_runtime_ptr);
    const uintptr_t map_addr = address_space_bridge->convert(addr, n);

    if (map_addr == Memory::AddressSpaceBridge::CONVERSION_ERROR)
        irrecoverable_error("%s: couldn't convert address %x", __PRETTY_FUNCTION__, addr);

    ::memcpy((void*)map_addr, src, n);
}

template <typename ptr_underlying_type>
template <typename T>
void Lptr<ptr_underlying_type>::write(const T&& t)
{
    memcpy(&t, sizeof(t));
}

template <typename ptr_underlying_type>
ptr_underlying_type Lptr<ptr_underlying_type>::operator*()
{
    const auto addr = reinterpret_cast<uintptr_t>(p_runtime_ptr);
    const auto n = sizeof(ptr_underlying_type);
    const uintptr_t map_addr = address_space_bridge->convert(addr, n);

    if (map_addr == Memory::AddressSpaceBridge::CONVERSION_ERROR)
        irrecoverable_error("%s: couldn't convert address %x", __PRETTY_FUNCTION__, addr);

    ptr_underlying_type ret;
    ::memcpy(&ret, (void*)map_addr, n);

    return ret;
}

template <typename ptr_underlying_type>
ptr_underlying_type Lptr<ptr_underlying_type>::operator[](uint n)
{
    return *(*this + n);
}

template <typename ptr_underlying_type>
template <typename Cast>
Lptr<Cast> Lptr<ptr_underlying_type>::convert_to() const
{
    return Lptr<Cast>(reinterpret_cast<Cast*>(p_runtime_ptr), address_space_bridge);
}

template <typename ptr_underlying_type>
typename Lptr<ptr_underlying_type>::ptr Lptr<ptr_underlying_type>::get_runtime_ptr() const
{
    return p_runtime_ptr;
}

template <typename ptr_underlying_type>
typename Lptr<ptr_underlying_type>::ptr Lptr<ptr_underlying_type>::get_load_addr(void* runtime_ptr, Memory::AddressSpaceBridge* address_space_bridge)
{
    const auto addr = reinterpret_cast<uintptr_t>(runtime_ptr);
    const uintptr_t map_addr = address_space_bridge->convert(addr, 1);

    if (map_addr == Memory::AddressSpaceBridge::CONVERSION_ERROR)
        irrecoverable_error("%s: couldn't convert address %x", __PRETTY_FUNCTION__, addr);

    return map_addr;
}
}
