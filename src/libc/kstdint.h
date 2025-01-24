#ifndef INCLUDE_STDINT_H
#define INCLUDE_STDINT_H
#if defined(__LP64__) || defined(_LP64)
typedef long int intmax_t;
typedef unsigned long int uintmax_t;
#else
typedef long long int intmax_t;
typedef unsigned long long int uintmax_t;

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;

#endif
#endif //INCLUDE_STDINT_H
