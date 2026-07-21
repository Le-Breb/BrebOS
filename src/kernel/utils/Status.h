#pragma once

class Status
{
protected:
    bool success_;

public:
    // ReSharper disable once CppNonExplicitConvertingConstructor
    Status(bool success_);
    static Status success();
    static Status failure();

    [[nodiscard]]
    bool ok() const;
    void except(const char* format, ...) const;
};