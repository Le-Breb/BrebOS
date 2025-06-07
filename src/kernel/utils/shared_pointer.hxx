#pragma once

#include "shared_pointer.h"

template <typename T>
SharedPointer<T>::SharedPointer(element_type* p)
    : data_(p)
    , count_(p == nullptr ? nullptr : new long{ 1 })
{}

template <typename T>
SharedPointer<T>::~SharedPointer()
{
    if (count_)
    {
        if (--(*count_) == 0)
        {
            delete data_;
            delete count_;
        }
    }
}

template <typename T>
SharedPointer<T>::SharedPointer(const SharedPointer<element_type>& other)
    : data_(other.data_)
    , count_(other.count_)
{
    if (count_)
        ++(*count_);
}

template <typename T>
void SharedPointer<T>::reset(element_type* p)
{
    if (p == data_)
        return;
    if (count_)
    {
        if (--(*count_) == 0)
        {
            delete data_;
            delete count_;
        }
    }
    data_ = p;
    count_ = p == nullptr ? nullptr : new long(1);
}

template <typename T>
SharedPointer<T>& SharedPointer<T>::operator=(const SharedPointer& other)
{
    if (count_)
    {
        if (--(*count_) == 0)
        {
            delete data_;
            delete count_;
        }
    }
    data_ = other.data_;
    count_ = other.count_;
    if (count_)
    {
        ++(*count_);
    }

    return *this;
}

template <typename T>
T& SharedPointer<T>::operator*() const
{
    return *data_;
}

template <typename T>
T* SharedPointer<T>::operator->() const
{
    return data_;
}

template <typename T>
typename SharedPointer<T>::element_type* SharedPointer<T>::get() const
{
    return data_;
}

template <typename T>
long SharedPointer<T>::use_count() const
{
    return count_ ? *count_ : 0;
}

template <typename T>
template <typename U>
bool SharedPointer<T>::operator!=(const SharedPointer<U>& rhs) const
{
    return data_ != rhs.data_;
}

template <typename T>
template <typename U>
bool SharedPointer<T>::operator==(const SharedPointer<U>& rhs) const
{
    return data_ == rhs.data_;
}

template <typename T>
bool SharedPointer<T>::operator==(const element_type* p) const
{
    return data_ == p;
}

template <typename T>
bool SharedPointer<T>::operator!=(const element_type* p) const
{
    return data_ != p;
}

template <typename T>
SharedPointer<typename SharedPointer<T>::element_type>&
SharedPointer<T>::operator=(SharedPointer&& other) noexcept
{
    if (this != &other)
    {
        if (count_)
        {
            if (--(*count_) == 0)
            {
                delete data_;
                delete count_;
            }
        }
        data_ = other.data_;
        count_ = other.count_;
        other.data_ = nullptr;
        other.count_ = nullptr;
    }
    return *this;
}

template <typename T>
SharedPointer<T>::SharedPointer(SharedPointer&& other)
    : data_(other.data_)
    , count_(other.count_)
{
    other.data_ = nullptr;
    other.count_ = nullptr;
}

template <typename T>
SharedPointer<T>::operator bool() const
{
    return data_ != nullptr;
}

template <typename T>
template <typename U>
bool SharedPointer<T>::is_a() const
{
    return dynamic_cast<U*>(data_);
}
