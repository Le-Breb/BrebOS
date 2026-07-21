#pragma once

#include "optional.h"

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
Optional<T>::operator bool()
{
    return data != nullptr;
}

template <typename T>
bool Optional<T>::operator==([[maybe_unused]] nullopt_t nullopt) const
{
    return is_null;
}
