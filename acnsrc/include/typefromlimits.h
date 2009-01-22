/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Engineering Arts (UK)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Engineering Arts nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	$Id$

*/
/************************************************************************/
#ifndef __typefromlimits_h__
#define __typefromlimits_h__ 1

/*
When inttypes.h is not available, this header attempts to work out what types
should be used from the ISO C89 header limits.h

This only tests for the most common cases on 8, 16, 32 and 64 bit platforms
*/

#include <limits.h>

/*
First try 8-bit char, 16-bit int, 32-bit long
*/
#if (SCHAR_MAX == 127) && (INT_MAX == 32767) && (LONG_MAX == 2147483647L)
#if !((UCHAR_MAX == 255) && (UINT_MAX == 65535) && (ULONG_MAX == 4294967295UL))
#error Can't resolve discrepancy between signed and unsigned sizes
#endif

#define __int8_t_defined
typedef signed char int8_t;
typedef unsigned char uint8_t;

#define __int16_t_defined
typedef int int16_t;
typedef unsigned int uint16_t;

#define __int32_t_defined
typedef long int int32_t;
typedef unsigned long int uint32_t;

#define __PRI8_PREFIX "hh"
#define __PRI16_PREFIX
#define __PRI32_PREFIX "l"

/* do we also have a 64-bit type? */
#if (LLONG_MAX == 9223372036854775807LL) && (ULLONG_MAX == 18446744073709551615ULL)

#define __int64_t_defined
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#define __PRI64_PREFIX "ll"

#endif  /* (LLONG_MAX == 9223372036854775807LL) && (ULONG_MAX == 18446744073709551615ULL) */

/*
Now try 8-bit char, 16-bit short, 32-bit int
*/
#elif (SCHAR_MAX == 127) && (SHRT_MAX == 32767) && (INT_MAX == 2147483647)
#if !((UCHAR_MAX == 255) && (USHRT_MAX == 65535) && (UINT_MAX == 4294967295U))
#error Can't resolve discrepancy between signed and unsigned sizes
#endif

#define __int8_t_defined
typedef signed char int8_t;
typedef unsigned char uint8_t;

#define __int16_t_defined
typedef short int16_t;
typedef unsigned short uint16_t;

#define __int32_t_defined
typedef int int32_t;
typedef unsigned int uint32_t;

#define __PRI8_PREFIX "hh"
#define __PRI16_PREFIX "h"
#define __PRI32_PREFIX

/* do we also have a 64-bit type? Could be long or long long */
#if (LONG_MAX == 2147483647) && (ULONG_MAX == 4294967295U)
/* long is 32 bitys - is LLONG_MAX defined */
#if defined(LLONG_MAX)
#if (LLONG_MAX == 9223372036854775807LL) && (ULLONG_MAX == 18446744073709551615ULL)
#define __int64_t_defined
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#define __PRI64_PREFIX "ll"
#endif  /* (LLONG_MAX == 9223372036854775807LL) && (ULLONG_MAX == 18446744073709551615ULL) */
#endif  /* defined(LLONG_MAX) */

#elif (LONG_MAX == 9223372036854775807L) && (ULONG_MAX == 18446744073709551615UL)

#define __int64_t_defined
typedef long int int64_t;
typedef unsigned long int uint64_t;
#define __PRI64_PREFIX "l"

#endif  /* (LONG_MAX == 9223372036854775807L) && (ULONG_MAX == 18446744073709551615UL) */
#endif  /* (SCHAR_MAX == 127) && (SHRT_MAX == 32767) && (INT_MAX == 2147483647) */

/* Printf specifiers are coerced to ints anyway */
#define PRId8 "d"
#define PRIi8 "i"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"

#define PRId16 "d"
#define PRIi16 "i"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"

/* But scanf needs a length  */
#define SCNd8 __PRI8_PREFIX "d"
#define SCNi8 __PRI8_PREFIX "i"
#define SCNu8 __PRI8_PREFIX "u"
#define SCNx8 __PRI8_PREFIX "x"

#define SCNd16 __PRI16_PREFIX "d"
#define SCNi16 __PRI16_PREFIX "i"
#define SCNu16 __PRI16_PREFIX "u"
#define SCNx16 __PRI16_PREFIX "x"

/* 32 bits may need a prefix for both print and scan */
#define PRId32 __PRI32_PREFIX "d"
#define PRIi32 __PRI32_PREFIX "i"
#define PRIu32 __PRI32_PREFIX "u"
#define PRIx32 __PRI32_PREFIX "x"
#define PRIX32 __PRI32_PREFIX "X"

#define SCNd32 __PRI32_PREFIX "d"
#define SCNi32 __PRI32_PREFIX "i"
#define SCNu32 __PRI32_PREFIX "u"
#define SCNx32 __PRI32_PREFIX "x"

#if defined(__PRI64_PREFIX)
#define PRId64 __PRI64_PREFIX "d"
#define PRIi64 __PRI64_PREFIX "i"
#define PRIu64 __PRI64_PREFIX "u"
#define PRIx64 __PRI64_PREFIX "x"
#define PRIX64 __PRI64_PREFIX "X"

#define SCNd64 __PRI64_PREFIX "d"
#define SCNi64 __PRI64_PREFIX "i"
#define SCNu64 __PRI64_PREFIX "u"
#define SCNx64 __PRI64_PREFIX "x"
#endif  /* defined(__PRI64_PREFIX) */

/*
Try and deduce pointer size
*/
#if (defined(__WORDSIZE) && __WORDSIZE == 64) || defined(_WIN64)

#define __intptr_t_defined
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;
#define __PRIPTR_PREFIX __PRI64_PREFIX

#elif (defined(__WORDSIZE) && __WORDSIZE == 32) || (defined(_WIN32) && !defined(_WIN64))

#define __intptr_t_defined
typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
#define __PRIPTR_PREFIX __PRI32_PREFIX

#endif  /* (defined(__WORDSIZE) && __WORDSIZE == 32) || (defined(_WIN64) && !defined(_WIN64)) */

#define PRIdPTR __PRIPTR_PREFIX "d"
#define PRIiPTR __PRIPTR_PREFIX "i"
#define PRIuPTR __PRIPTR_PREFIX "u"
#define PRIxPTR __PRIPTR_PREFIX "x"
#define PRIXPTR __PRIPTR_PREFIX "X"

#define SCNdPTR __PRIPTR_PREFIX "d"
#define SCNiPTR __PRIPTR_PREFIX "i"
#define SCNuPTR __PRIPTR_PREFIX "u"
#define SCNxPTR __PRIPTR_PREFIX "x"

#endif /* ifndef __typefromlimits_h__ */
