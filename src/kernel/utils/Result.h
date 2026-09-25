#pragma once

#include <utility>
#include <type_traits>
#include <cstdarg>
#include <stddef.h>
#include "Status.h"
#include <new>

[[noreturn]]
__attribute__ ((format (printf, 1, 2)))
extern int irrecoverable_error(const char* format, ...);
__attribute__ ((format (printf, 1, 2)))
extern int printf_warn(const char* format, ...);
extern char* strdup(const char* s);
extern int sprintf_aux(char* str, const char* format, va_list list);
extern int sprintf(char* str, const char* format, ...);

template <typename T>
class Result
{
    const char* msg;
    const bool has_value;
    alignas(T) char data[sizeof(T)];

    explicit Result(const char* msg) : msg(strdup(msg)), has_value(false) {}
    explicit Result(const T& value) : msg(nullptr), has_value(true)
    {
        new (data) T(value);
    }
    // Constrained so it never hijacks copy/move construction from a non-const Result lvalue
    // Example scenario:
    // Result<SharedPointer<Dentry>> a = ...;
    // Result<SharedPointer<Dentry>> b(a);   // a is a non-const lvalue
    // In this scenario Args&& constructor beats the copy constructor, as copy constructor takes const T&, which requires
    // adding const, which is more expansive than calling the exactly matching Args&& constructor.
    // Yeah that's fucked up, C++ is a wonderful language!
    template <typename... Args>
        requires (!(sizeof...(Args) == 1 && (std::is_same_v<std::remove_cvref_t<Args>, Result> && ...)))
    explicit Result(Args&&... args) : msg(nullptr), has_value(true)
    {
        new (data) T(std::forward<Args>(args)...);
    }
public:
    Result(const Result& other) : msg(other.msg ? strdup(other.msg) : nullptr), has_value(other.has_value)
    {
        if (has_value)
            new (data) T(*reinterpret_cast<const T*>(other.data));
    }
    Result(Result&& other) noexcept : msg(other.msg), has_value(other.has_value)
    {
        other.msg = nullptr;
        if (has_value)
            new (data) T(std::move(*reinterpret_cast<T*>(other.data)));
    }

    Result(const Status& status) : msg(status.is_ok() ? nullptr : strdup(status.err().what())), has_value(false)
    {
        if (status.is_ok())
            irrecoverable_error("Cannot build a result out of a successful status");
    }
    Result(Err&& err)  : msg(err.release()), has_value(false) {}

    static Result ok(const T& value) { return Result(value); }
    template <typename... Args>
    static Result ok(Args&&... args) { return Result(std::forward<Args>(args)...); };

    // Creates a result containing a formatted error. Do not use this function directly, instead use MAKE_ERROR
    static Result error(const char* format, va_list list)
    {
        static char buffer[Status::MAX_MSG_LEN];
        const int n = sprintf_aux(buffer, format, list);
        if (n >= Status::MAX_MSG_LEN)
            irrecoverable_error("Result::error: message too long");
        buffer[n] = '\0'; // sprintf_aux does not null-terminate
        return Result(buffer);
    }

    ~Result()
    {
        if (has_value)
            reinterpret_cast<T*>(data)->~T();
        delete msg;
    }

    T&& expect() &&
    {
        if (!has_value)
            irrecoverable_error("Result::except: %s", msg);
        return std::move(*reinterpret_cast<T*>(data));
    }

    bool warn_is_ok() const
    {
        if (!has_value)
        {
            printf_warn("Result::warn_if_err: %s", msg);
            return false;
        }
        return true;
    }

    // Returns an owning copy of the message, so that the Err and this Result can each free their own
    Err err() const { return Err(msg ? strdup(msg) : nullptr); }

    [[nodiscard]]
    bool is_ok() const { return has_value; }
};

// Helper wrappers around ok and error to avoid having to specify Result template parameter and let the compiler
// deduce it

template <typename T>
Result<T> make_ok(T value)
{
    return Result<T>::ok(value);
}

template<typename T, typename... Args>
Result<T> make_ok(Args&&... args)
{
    return Result<T>::ok(std::forward<Args>(args)...);
}

__attribute__ ((format (printf, 3, 4)))
inline Err make_err_at(const char* file, int line, const char* format, ...)
{
    char msg[Status::MAX_MSG_LEN];
    va_list list;
    va_start(list, format);
    const int n = sprintf_aux(msg, format, list);
    if (n >= Status::MAX_MSG_LEN)
        irrecoverable_error("make_err: message too long");
    msg[n] = '\0';
    va_end(list);

    char* full = new char[Status::MAX_MSG_LEN];
    const int n2 = sprintf(full, "%s:%d: %s", file, line, msg);
    if (n2 >= Status::MAX_MSG_LEN)
        irrecoverable_error("make_err: message too long");
    full[n2] = '\0';

    return Err(full);
}

#define MAKE_ERR(format, ...) make_err_at(__FILE__, __LINE__, format __VA_OPT__(,) __VA_ARGS__)