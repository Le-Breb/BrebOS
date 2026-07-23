#pragma once
#include <stdint.h>
#include <stddef.h>

#define FNV1A_32_INIT ((Fnv32_t)0x811c9dc5)

/*
 * 32 bit magic FNV-1a prime
 */
#define FNV_32_PRIME ((Fnv32_t)0x01000193)

template<typename>
inline constexpr bool always_false = false;

template<typename T>
struct Hash;

typedef uint32_t Fnv32_t;

Fnv32_t
fnv_32a_buf(const void *buf, size_t len, Fnv32_t hval);

Fnv32_t
fnv_32a_str(const char *str, Fnv32_t hval);

template<>
struct Hash<char*>
{
    uint32_t operator()(const char* str) const;
};

#include "Hash.hxx"
