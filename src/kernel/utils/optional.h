#pragma once

struct nullopt_t {
    constexpr explicit nullopt_t(int) {}
};
inline constexpr nullopt_t nullopt{0};

template <typename T>
class Optional
{
    alignas(sizeof(T)) char data[sizeof(T)];
    bool is_null;

public:
    Optional(const T& t); // NOLINT(*-explicit-constructor)
    Optional();

    T& operator*();
    T& operator->();
    operator bool(); // NOLINT(*-explicit-constructor)
    bool operator==(nullopt_t nullopt);
};

#include "optional.hxx"
related code