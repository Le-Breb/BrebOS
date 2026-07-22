#pragma once
#include <stdint.h>

template<typename>
inline constexpr bool always_false = false;

template<typename T>
struct Hash;

typedef uint32_t Fnv32_t;

Fnv32_t
fnv_32a_buf(void *buf, size_t len, Fnv32_t hval);

Fnv32_t
fnv_32a_str(char *str, Fnv32_t hval);

template<>
struct Hash<char*>
{
    uint32_t operator()(const char* str) const;
};

#include "Hash.hxx"
