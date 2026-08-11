#pragma once

#include "optional.h"
#include <stdarg.h>
#include <new>

#define __opt_ptr reinterpret_cast<T*>(data)

template <typename T>
Optional<T>::Optional(const T& t) : is_null(false)
{
    new (&data)T(t);
}

template <typename T>
Optional<T>::Optional([[maybe_unused]] const nullopt_t& nullopt) : is_null(true)
{

}

template <typename T>
Optional<T>::Optional(const Optional& other) : is_null(other.is_null)
{
    if (!is_null)
        new (&data)T(*reinterpret_cast<const T*>(other.data));
}

template <typename T>
Optional<T>& Optional<T>::operator=(const Optional& other)
{
    if (this == &other)
        return *this;

    if (!is_null)
        __opt_ptr->~T();
    is_null = other.is_null;
    if (!is_null)
        new (&data)T(*reinterpret_cast<const T*>(other.data));
    return *this;
}

template <typename T>
Optional<T>::~Optional()
{
    if (!is_null)
        __opt_ptr->~T();
}

template <typename T>
T& Optional<T>::operator*()
{
    return *__opt_ptr;
}

template <typename T>
T* Optional<T>::operator->()
{
    return __opt_ptr;
}

template <typename T>
Optional<T>::operator bool() const
{
    return !is_null;
}

template <typename T>
bool Optional<T>::operator==([[maybe_unused]] nullopt_t nullopt) const
{
    return is_null;
}

[[noreturn]]
extern void irrecoverable_error_aux(const char* format, va_list list);

template <typename T>
T& Optional<T>::expect(const char* format, ...)
{
    if (is_null)
    {
        va_list list;
        va_start(list, format);
        irrecoverable_error_aux(format, list);
        va_end(list);
    }

    return *__opt_ptr;
}
