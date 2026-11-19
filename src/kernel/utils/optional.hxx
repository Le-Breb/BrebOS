#pragma once

#include "optional.h"
#include <stdarg.h>
#include <utility>
#include <new>

template <typename T>
template <typename ... Args> requires (sizeof...(Args) >= 1 &&
    !(sizeof...(Args) == 1 && (std::is_same_v<std::remove_cvref_t<Args>, Optional<T>> && ...)))
Optional<T>::Optional(Args&&... args) : is_null(false)
{
    new (&data)T(std::forward<Args>(args)...);
}

template <typename T>
Optional<T>::Optional(const T& t) : is_null(false)
{
    new (&data)T(t);
}

template <typename T>
Optional<T>::Optional(T&& t) : is_null(false)
{
    new (&data)T(std::move(t));
}

template <typename T>
Optional<T>::Optional([[maybe_unused]] const nullopt_t& nullopt) : is_null(true)
{

}

template <typename T>
Optional<T>::Optional(const Optional& other) : is_null(other.is_null)
{
    if (!is_null)
        new (&data)T(*other.ptr());
}

template <typename T>
Optional<T>& Optional<T>::operator=(const Optional& other)
{
    if (this == &other)
        return *this;

    if (!is_null)
        ptr()->~T();
    is_null = other.is_null;
    if (!is_null)
        new (&data)T(*other.ptr());
    return *this;
}

template <typename T>
Optional<T>::~Optional()
{
    if (!is_null)
        ptr()->~T();
}

template <typename T>
const T& Optional<T>::operator*() const
{
    return *ptr();
}

template <typename T>
T& Optional<T>::operator*()
{
    return *ptr();
}

template <typename T>
T* Optional<T>::operator->()
{
    return ptr();
}

template <typename T>
const T* Optional<T>::operator->() const
{
    return ptr();
}

template <typename T>
Optional<T>::operator bool() const
{
    return !is_null;
}

template <typename T>
bool Optional<T>::operator==(const Optional& other) const
{
    return (is_null && other.is_null) || (!is_null && !other.is_null && *ptr() == *other.ptr());
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

    return *ptr();
}
