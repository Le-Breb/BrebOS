#pragma once

#include "Hash.h"

inline uint32_t Hash<char*>::operator()(const char* str) const
{
    return fnv_32a_str((char*)str, FNV1A_32_INIT);
}
