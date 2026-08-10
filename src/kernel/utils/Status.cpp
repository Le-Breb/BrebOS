#include "Status.h"
#include <stdarg.h>
#include <stddef.h>

[[noreturn]]
__attribute__ ((format (printf, 1, 2)))
extern int irrecoverable_error(const char* format, ...);
extern char* strdup(const char* s);
extern int sprintf_aux(char* str, const char* format, va_list list);

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
    if (sprintf_aux(buffer, format, list) > MAX_MSG_LEN)
        irrecoverable_error("Status::failure: message too long");
    va_end(list);

    return Status(buffer);
}

bool Status::ok() const
{
    return success_;
}

const char* Status::get_msg() const
{
    return msg;
}

void Status::expect() const
{
    if (!success_)
        irrecoverable_error("Status::expect: %s", msg);
}

[[noreturn]]
extern void irrecoverable_error_aux(const char* format, va_list list);
