#include "Status.h"
#include <stdarg.h>
#include <stddef.h>

[[noreturn]]
__attribute__ ((format (printf, 1, 2)))
extern int irrecoverable_error(const char* format, ...);
extern char* strdup(const char* s);
extern int sprintf_aux(char* str, const char* format, va_list list);
__attribute__ ((format (printf, 1, 2)))
extern int printf_warn(const char* format, ...);

Status::Status(const char* msg) : success_(false), msg(strdup(msg))
{

}

Status::Status() : success_(true), msg(nullptr)
{

}

Status Status::success()
{
    return Status();
}

Status Status::failure(const char* format, ...)
{
    static char buffer[MAX_MSG_LEN];
    va_list list;
    va_start(list, format);
    const int n = sprintf_aux(buffer, format, list);
    va_end(list);
    if (n >= MAX_MSG_LEN)
        irrecoverable_error("Status::failure: message too long");
    buffer[n] = '\0'; // sprintf_aux does not null-terminate

    return Status(buffer);
}

Status::Status(Err&& err) : success_(false), msg(err.release())
{
}


bool Status::is_ok() const
{
    return success_;
}

Err Status::err() const
{
    return Err(msg ? strdup(msg) : nullptr);
}

void Status::expect() const
{
    if (!success_)
        irrecoverable_error("Status::expect: %s", msg);
}

bool Status::warn_is_ok() const
{
    if (!success_)
    {
        printf_warn("Result::warn_if_err: %s", msg);
        return false;
    }
    return true;
}

[[noreturn]]
extern void irrecoverable_error_aux(const char* format, va_list list);
