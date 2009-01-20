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
  
  For ISO C99 compilers this should just pull in inttypes.h
  For C89 we can deduce them mostly from limits.h
  If USER_DEFINE_INTTYPES is set the user wants to define these themselves
*/

#if USER_DEFINE_INTTYPES
#include "user_types.h"

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <inttypes.h>
#include <stdbool.h>

/*
_MSC_VER is still defined when VisualC is not in ISO mode
(and limits.h still works).
*/
#elif defined(__STDC__) || defined(_MSC_VER)
#include "typefromlimits.h"

/*
  Booleans
  This is the same as stdbool.h
*/
#ifndef bool
#define bool _Bool
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif
#define __bool_true_false_are_defined	1

#endif  /* defined(__STDC__) || defined(_MSC_VER) */

#ifndef OK
#define OK 0
#endif

#ifndef FAIL
#define FAIL -1
#endif

#ifndef HAVE_ip4addr_t
  typedef uint32_t ip4addr_t; /* ip address as a long */
  #define HAVE_ip4addr_t
#endif

#define PACKED __attribute__((__packed__))

#define UNUSED_ARG(x) (void)(x)
#define _Bool int8_t

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define ZEROARRAY
#else
#define ZEROARRAY 1
#endif

#endif /* __types_h__ */
