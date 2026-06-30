#pragma once

#include "TmpString.h"

typedef unsigned int uint;

/**
 * Temporary string. Data is allocated using slab allocators.
 * Useful to quickly allocate temporary strings
 */
class TmpString
{
    uint size;
    char* data;
public:
    explicit TmpString(uint size);
    ~TmpString();

    // Copy constructors
    TmpString(const TmpString& other);
    TmpString& operator=(const TmpString& other);
    // Move constructors
    TmpString(TmpString&& other) noexcept;
    TmpString& operator=(TmpString&& other) noexcept;

    // Retrieve the underlying string
    char* operator*() const;

    // Append another tmp string
    TmpString& concat(const TmpString& other);
};