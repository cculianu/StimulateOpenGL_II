#ifndef TypeDefs_H
#define TypeDefs_H

#ifndef Q_OS_WIN
#  include <stdint.h>
#else
  typedef signed short int16_t;
  typedef unsigned short uint16_t;
  typedef long long int int64_t;
  typedef unsigned long long int uint64_t;
  typedef signed int int32_t;
  typedef unsigned int uint32_t;
#endif

typedef int16_t  int16;
typedef uint16_t uint16;
typedef int64_t  s64;
typedef uint64_t u64;
typedef s64 i64;
typedef uint32_t uint32;
#ifndef HAVE_NIDAQmx
typedef int32_t int32;
#endif
typedef int32_t i32;
typedef int32_t s32;
typedef uint32_t u32;
typedef double float64;

#endif
