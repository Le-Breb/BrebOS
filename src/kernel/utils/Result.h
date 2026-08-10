#pragma once

#include <utility>
#include <cstdarg>
#include <stddef.h>
#include "Status.h"
#include <new>

[[noreturn]]
__attribute__ ((format (printf, 1, 2)))
extern int irrecoverable_error(const char* format, ...);
extern char* strdup(const char* s);
extern int sprintf_aux(char* str, const char* format, va_list list);

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
    explicit Result(const Status& status) : msg(status.ok() ? nullptr : strdup(status.get_msg())), has_value(false)
    {
        if (status.ok())
            irrecoverable_error("Cannot build a result out of a successful status");
    }
    static Result ok(const T& value) { return Result(value); }
    static Result error(const char* format, ...) {
        static char buffer[Status::MAX_MSG_LEN];
        va_list list;
        va_start(list, format);
        if (sprintf_aux(buffer, format, list) > Status::MAX_MSG_LEN)
            irrecoverable_error("Result::error: message too long");
        va_end(list);
        return Result(buffer);
    }
    ~Result()
    {
        if (has_value)
            reinterpret_cast<T*>(data)->~T();
        delete msg;
    }
    T&& expect()
    {
        if (!has_value)
            irrecoverable_error("Result::except: %s", msg);
        return std::move(*reinterpret_cast<T*>(data));
    }
};