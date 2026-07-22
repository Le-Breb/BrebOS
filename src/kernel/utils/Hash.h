#pragma once
#include <stdint.h>

template<typename>
inline constexpr bool always_false = false;

template<typename T>
struct Hash;

template<>
struct Hash<char*>
{
    uint32_t operator()(const char* str) const;
};

#include "Hash.hxx"
