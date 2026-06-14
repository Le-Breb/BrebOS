#pragma once

#include "ELFTools.h"

#include "../utils/comparison.h"
#include "../core/fb.h"
#include "kstring.h"

typedef unsigned int uint;

namespace ELFTools
{
    template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>::Lptr(ptr runtime_ptr, const list<alloc>* address_space_manager) : p_runtime_ptr(runtime_ptr), allocations(address_space_manager)
{
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type>& Lptr<ptr_underlying_type>::operator=(Elf32_Addr runtime_addr)
{
    p_runtime_ptr = static_cast<ptr>(runtime_addr);
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
    return Lptr{p_runtime_ptr + n, allocations};
}

template <typename ptr_underlying_type>
Lptr<ptr_underlying_type> Lptr<ptr_underlying_type>::operator+(unsigned int n) const
{
    return {p_runtime_ptr + n, allocations};
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
    return {p_runtime_ptr - n, allocations};
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
    for (const auto& alloc : *allocations)
    {
        if (alloc.alloc_.start <= addr && alloc.alloc_.end > addr)
        {
            if (alloc.alloc_.end > addr + n)
            {
                ::memset((void*)(alloc.load_addr + (addr - alloc.alloc_.start)), c, n);
                return;
            }
            irrecoverable_error("%s: allocation where lies ptr does not cover n bytes (ie memory region crosses allocations)."
                                "This isn't necessarily an error, but it is not supported and seriously suspicious",
                                __PRETTY_FUNCTION__);
        }
    }

    irrecoverable_error("%s: runtime to load address conversion failure", __PRETTY_FUNCTION__);
}

template <typename ptr_underlying_type>
void Lptr<ptr_underlying_type>::memcpy(const void* src, size_t n) const
{
    const auto addr = reinterpret_cast<uintptr_t>(p_runtime_ptr);
    for (const auto& alloc : *allocations)
    {
        if (alloc.alloc_.start <= addr && alloc.alloc_.end > addr)
        {
            if (alloc.alloc_.end > addr + n)
            {
                ::memcpy((void*)(alloc.load_addr + (addr - alloc.alloc_.start)), src, n);
                return;
            }
            irrecoverable_error("%s: allocation where lies ptr does not cover n bytes (ie memory region crosses allocations)."
                                "This isn't necessarily an error, but it is not supported and seriously suspicious",
                                __PRETTY_FUNCTION__);
        }
    }

    irrecoverable_error("%s: runtime to load address conversion failure", __PRETTY_FUNCTION__);
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
    for (const auto& alloc : *allocations)
    {
        if (alloc.alloc_.start <= addr && alloc.alloc_.end > addr)
        {
            if (const auto n = sizeof(ptr_underlying_type); alloc.alloc_.end > addr + n)
            {
                ptr_underlying_type ret;
                ::memcpy(&ret, (void*)(alloc.load_addr + (addr - alloc.alloc_.start)), n);
                return ret;
            }
            irrecoverable_error("%s: allocation where lies ptr does not cover n bytes (ie memory region crosses allocations)."
                                "This isn't necessarily an error, but it is not supported and seriously suspicious",
                                __PRETTY_FUNCTION__);
        }
    }

    irrecoverable_error("%s: runtime to load address conversion failure", __PRETTY_FUNCTION__);
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
    return Lptr<Cast>(reinterpret_cast<Cast*>(p_runtime_ptr), allocations);
}

template <typename ptr_underlying_type>
typename Lptr<ptr_underlying_type>::ptr Lptr<ptr_underlying_type>::get_runtime_ptr() const
{
    return p_runtime_ptr;
}

template <typename ptr_underlying_type>
typename Lptr<ptr_underlying_type>::ptr Lptr<ptr_underlying_type>::get_load_addr(void* runtime_ptr, const list<alloc>& allocations)
{
    const auto addr = reinterpret_cast<uintptr_t>(runtime_ptr);
    for (const auto& alloc : allocations)
    {
        if (alloc.alloc_.start <= addr && alloc.alloc_.end > addr)
            return reinterpret_cast<ptr>(alloc.load_addr + (addr - alloc.alloc_.start));
    }

    irrecoverable_error("%s: runtime to load address conversion failure", __PRETTY_FUNCTION__);
}
}
