#pragma once

#include <utility>
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
public:
    Result(const Status& status) : msg(status.is_ok() ? nullptr : strdup(status.err().what())), has_value(false)
    {
        if (status.is_ok())
            irrecoverable_error("Cannot build a result out of a successful status");
    }
    Result(Err&& err)  : msg(err.release()), has_value(false) {}

    static Result ok(const T& value) { return Result(value); }

    // Creates a result containing a formatted error. Do not use this function directly, instead use MAKE_ERROR
    static Result error(const char* format, va_list list)
    {
        static char buffer[Status::MAX_MSG_LEN];
        if (sprintf_aux(buffer, format, list) > Status::MAX_MSG_LEN)
            irrecoverable_error("Result::error: message too long");
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

    Err err() const { return Err(msg); }

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

inline Err make_err_at(const char* file, int line, const char* format, ...)
{
    char msg[Status::MAX_MSG_LEN];
    va_list list;
    va_start(list, format);
    int n = sprintf_aux(msg, format, list);
    va_end(list);
    if (n > Status::MAX_MSG_LEN)
        irrecoverable_error("make_err: message too long");

    char* full = new char[Status::MAX_MSG_LEN];
    if (sprintf(full, "%s:%d: %s", file, line, msg) > Status::MAX_MSG_LEN)
        irrecoverable_error("make_err: message too long");

    return Err(full);
}

#define MAKE_ERR(format, ...) make_err_at(__FILE__, __LINE__, format __VA_OPT__(,) __VA_ARGS__)