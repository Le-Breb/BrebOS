#ifndef CUSTOM_OS_CSTDDEF_H
#define CUSTOM_OS_CSTDDEF_H

#define NULL 0
typedef unsigned long size_t;
typedef unsigned int uint;
typedef typeof((int*) 0x00 - (int*) 0x00) ptrdiff_t;
#endif //CUSTOM_OS_CSTDDEF_H
