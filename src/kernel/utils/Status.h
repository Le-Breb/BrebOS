#pragma once

class Status
{
protected:
    bool success_;
    const char* msg;

    Status(const char* msg);
    Status();
public:
    // ReSharper disable once CppNonExplicitConvertingConstructor
    static Status success();
    static Status failure(const char* format, ...);

    [[nodiscard]]
    bool ok() const;

    [[nodiscard]]
    const char* get_msg() const;

    void expect() const;

    static constexpr int MAX_MSG_LEN = 200;
};