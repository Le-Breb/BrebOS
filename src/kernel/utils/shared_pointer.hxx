#pragma once

#include "shared_pointer.h"

template <typename T>
SharedPointer<T>::SharedPointer(element_type* p)
    : data_(p)
    , cb_(nullptr)
{
    if (p != nullptr)
        cb_ = new ControlBlock{1, &delete_data};
}

template <typename T>
SharedPointer<T>::SharedPointer(decltype(nullptr))
    : data_(nullptr)
    , cb_(nullptr)
{}

template <typename T>
void SharedPointer<T>::delete_data(void* p)
{
    delete static_cast<T*>(p);
}

template <typename T>
void SharedPointer<T>::release()
{
    if (cb_ && --cb_->count == 0)
    {
        cb_->destroy(data_);
        delete cb_;
    }
}

template <typename T>
SharedPointer<T>::~SharedPointer()
{
    release();
}

template <typename T>
SharedPointer<T>::SharedPointer(const SharedPointer<element_type>& other)
    : data_(other.data_)
    , cb_(other.cb_)
{
    if (cb_)
        ++cb_->count;
}

template <typename T>
void SharedPointer<T>::reset(element_type* p)
{
    if (p == data_)
        return;
    release();
    data_ = p;
    cb_ = p == nullptr ? nullptr : new ControlBlock{1, &delete_data};
}

template <typename T>
SharedPointer<T>& SharedPointer<T>::operator=(const SharedPointer& other)
{
    if (other.cb_)
        ++other.cb_->count; // Before release, in case of self-assignment
    release();
    data_ = other.data_;
    cb_ = other.cb_;

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
    return cb_ ? cb_->count : 0;
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
        release();
        data_ = other.data_;
        cb_ = other.cb_;
        other.data_ = nullptr;
        other.cb_ = nullptr;
    }
    return *this;
}

template <typename T>
SharedPointer<T>::SharedPointer(SharedPointer&& other)
    : data_(other.data_)
    , cb_(other.cb_)
{
    other.data_ = nullptr;
    other.cb_ = nullptr;
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
