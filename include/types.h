/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Pathway Connectivity Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Pathway Connectivity Inc. nor the names of its
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
/*--------------------------------------------------------------------*/
#ifndef __types_h__
#define __types_h__ 1
/*
  Type definitions for fixed size types in 8, 16, 32 bits

  These depend on limits.h which should be present in any ANSI C implementation
  If any of the definitions here do not work then put your own in user_types.h
  not here. user_types.h is included at the end of this header to supply any
  definitions not automatically assigned
*/


/*
Work out what we can automatically
*/
#include <limits.h>
#include "have_types.h"
/*
#include <stddef.h>
*/

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef OK
#define OK 0
#endif

#ifndef FAIL
#define FAIL -1
#endif


/* 8-bit types */
#ifndef __HAVE_uint8_t
#if (UCHAR_MAX == 255)
typedef unsigned char uint8_t;
#define __HAVE_uint8_t 1
#endif
#endif

#ifndef __HAVE_sint8_t
#if (SCHAR_MAX == 127)
typedef signed char sint8_t;
#define __HAVE_sint8_t 1
#endif
#endif

#ifndef __HAVE_int8_t
typedef sint8_t int8_t;
#define __HAVE_int8_t 1
#endif

/* 16-bit types */
#ifndef __HAVE_uint16_t
  #if (UINT_MAX == 65535)
    typedef unsigned int uint16_t;
    #define __HAVE_uint16_t 1
  #elif (USHRT_MAX == 65535)
    typedef unsigned short uint16_t;
    #define __HAVE_uint16_t 1
  #endif
#endif

#ifndef __HAVE_sint16_t
  #if (INT_MAX == 32767)
    typedef int sint16_t;
    #define __HAVE_sint16_t 1
  #elif (SHRT_MAX == 32767)
    typedef short sint16_t;
    #define __HAVE_sint16_t 1
  #endif
#endif

/* 32-bit types */
#ifndef __HAVE_uint32_t
  #ifndef uint32_t
    #if (ULONG_MAX == 4294967295UL)
      typedef unsigned long int uint32_t;
      #define __uint32_t_defined /* for linux*/
      #define __HAVE_uint32_t 1
    #elif (UINT_MAX == 4294967295UL)
      typedef unsigned int uint32_t; /* this is used for netburner */
      #define __HAVE_uint32_t 1
    #endif
  #endif
#endif


#ifndef __HAVE_sint32_t
  #if (INT_MAX == 2147483647L)
    typedef int sint32_t;
    #define __HAVE_sint32_t 1
  #elif (LONG_MAX == 2147483647L)
    typedef long int sint32_t;
    #define __HAVE_sint32_t 1
  #endif
#endif

/* 64-bit types */
#if defined(NEED_INT64)
  #if (ULONG_MAX == 18446744073709551615ULL)
    typedef unsigned long int uint64_t;
    #define __HAVE_uint64_t 1
  #elif (ULLONG_MAX == 18446744073709551615ULL)
    typedef unsigned long long int uint64_t;
    #define __HAVE_uint64_t 1
  #endif
  #if (LONG_MAX == 9223372036854775807LL)
    typedef long int sint64_t;
    #define __HAVE_sint64_t 1
  #elif (LLONG_MAX == 9223372036854775807LL)
    typedef long long int sint64_t;
    #define __HAVE_sint64_t 1
  #endif
#endif /* #ifdef NEED_INT64 */

#ifndef HAVE_ip4addr_t
  typedef uint32_t ip4addr_t; /* ip address as a long */
  #define HAVE_ip4addr_t
#endif

#define PACKED __attribute__((__packed__))
#define UNUSED_ARG(x) (void)(x)
#define bool uint8_t

typedef struct p_string_s
{
	uint16_t length;
	uint8_t  *value;   /* value[0]; */
} p_string_t;

#include "user_types.h"

#endif /* __types_h__ */
