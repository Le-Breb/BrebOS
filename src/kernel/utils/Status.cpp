#include "Status.h"
#include <stdarg.h>

Status::Status(bool success_) : success_(success_)
{

}

Status Status::success()
{
    return Status(true);
}

Status Status::failure()
{
    return Status(false);
}

bool Status::ok() const
{
    return success_;
}

[[noreturn]]
extern void irrecoverable_error_aux(const char* format, va_list list);

void Status::except(const char* format, ...) const
{
    if (!success_)
    {
        va_list list;
        va_start(list, format);
        irrecoverable_error_aux(format, list);
        va_end(list);
    }
}
