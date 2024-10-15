#ifndef INCLUDE_STDINT_H
#define INCLUDE_STDINT_H
#if defined(__LP64__) || defined(_LP64)
typedef long int intmax_t;
typedef unsigned long int intmax_t;
#else
typedef long long int intmax_t;
typedef unsigned long long int uintmax_t;
typedef long long int int64_t;

#endif
#endif //INCLUDE_STDINT_H
