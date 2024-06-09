#ifndef CUSTOM_OS_CSTDDEF_H
#define CUSTOM_OS_CSTDDEF_H
typedef unsigned int size_t;
typedef typeof((int*) 0x00 - (int*) 0x00) ptrdiff_t; // valid since C23
#endif //CUSTOM_OS_CSTDDEF_H
