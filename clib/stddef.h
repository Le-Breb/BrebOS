#ifndef CUSTOM_OS_CSTDDEF_H
#define CUSTOM_OS_CSTDDEF_H

#define NULL 0
typedef unsigned int size_t;
typedef unsigned int uint;
typedef typeof((int*) 0x00 - (int*) 0x00) ptrdiff_t; // valid since C23
#endif //CUSTOM_OS_CSTDDEF_H
