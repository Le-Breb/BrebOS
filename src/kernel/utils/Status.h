#pragma once

#define TRY(expr)                                  \
    ({                                              \
        auto&& _try_res = (expr);                  \
        if (!_try_res.is_ok())                      \
            return _try_res.err();                  \
        std::move(_try_res).expect();                \
    })

class Err
{
    const char* msg;
public:
    explicit Err(const char* msg) : msg(msg) {}

    Err(const Err&) = delete;
    Err& operator=(const Err&) = delete;
    Err(Err&& other) noexcept : msg(other.msg) { other.msg = nullptr; }
    Err& operator=(Err&&) = delete;

    ~Err() { delete msg; }

    const char* what() const { return msg; }

    // transfers ownership out; caller becomes responsible for freeing
    const char* release() { const char* m = msg; msg = nullptr; return m; }
};

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

    Status(Err&& err);

    [[nodiscard]]
    bool is_ok() const;

    [[nodiscard]]
    Err err() const;

    void expect() const;

    bool warn_is_ok() const;

    static constexpr int MAX_MSG_LEN = 200;
};