#pragma once

#include <new>
#include <type_traits>

struct nullopt_t {
    constexpr explicit nullopt_t(int) {}
};

inline constexpr nullopt_t nullopt{0};

template <typename T>
class Optional
{
    alignas(T) char data[sizeof(T)];
    bool is_null;

    T* ptr() { return std::launder(reinterpret_cast<T*>(data)); }
    const T* ptr() const { return std::launder(reinterpret_cast<const T*>(data)); }

public:
    Optional() : is_null(true) {}
    template <typename... Args>
        requires (sizeof...(Args) >= 1 &&
                  !(sizeof...(Args) == 1 && (std::is_same_v<std::remove_cvref_t<Args>, Optional> && ...)))
    Optional(Args&&... args);
    Optional(const T& t); // NOLINT(*-explicit-constructor)
    Optional(T&& t); // NOLINT(*-explicit-constructor)
    Optional(const nullopt_t& nullopt);
    Optional(const Optional& other);
    Optional& operator=(const Optional& other);
    ~Optional();

    const T& operator*() const;
    T& operator*();
    T* operator->();
    const T* operator->() const;
    operator bool() const; // NOLINT(*-explicit-constructor)
    bool operator==(const Optional& other) const;
    bool operator==(nullopt_t nullopt) const;
    T& expect(const char* format, ...);
};

#include "optional.hxx"