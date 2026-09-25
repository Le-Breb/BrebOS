#pragma once

#define TRY(expr)                                  \
    ({                                              \
        auto&& _try_res = (expr);                  \
        if (!_try_res.is_ok())                      \
            return std::move(_try_res).take_err();  \
        std::move(_try_res).expect();                \
    })

#define TRY_OR_RETURN(expr, alt)                          \
    ({                                              \
        auto&& _try_res = (expr);                  \
        if (!_try_res.is_ok())                      \
            return alt;                             \
        std::move(_try_res).expect();                \
    })

#define TRY_OR(expr, alt)                                   \
    ({                                                            \
        auto&& _try_res = (expr);                                 \
        _try_res.is_ok() ? std::move(_try_res).expect()           \
                              : (alt);                            \
    })

#define WARN_TRY_OR_RETURN(expr, alt)                     \
    ({                                              \
        auto&& _try_res = (expr);                  \
        if (!_try_res.warn_is_ok())                 \
            return alt;                             \
        std::move(_try_res).expect();                \
    })

#define WARN_TRY_OR(expr, alt)                              \
    ({                                                            \
        auto&& _try_res = (expr);                                 \
        _try_res.warn_is_ok() ? std::move(_try_res).expect()      \
                              : (alt);                            \
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

    const char* what() const { return msg ? msg : "(no error message)"; }

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

    Status(const Status& other);
    Status(Status&& other) noexcept;
    Status& operator=(const Status&) = delete;
    Status& operator=(Status&&) = delete;

    ~Status();

    [[nodiscard]]
    bool is_ok() const;

    [[nodiscard]]
    const char* err_msg() const;

    [[nodiscard]]
    Err take_err() &&;

    void expect() const;

    bool warn_is_ok() const;

    static constexpr int MAX_MSG_LEN = 200;
};