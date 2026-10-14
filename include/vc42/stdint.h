/* stdint.h for Visual C++ 4.2, which predates C99. Put include/vc42 FIRST on
 * the INCLUDE path only when building with the original compiler
 * (match/config.py does, so a modern header set is never shadowed by
 * accident). */
#ifndef DF_VC42_STDINT_H
#define DF_VC42_STDINT_H

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;
typedef int                intptr_t;
typedef unsigned int       uintptr_t;

#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  0xff
#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 0xffff
#define INT32_MIN  (-2147483647 - 1)
#define INT32_MAX  2147483647
#define UINT32_MAX 0xffffffff

#endif
