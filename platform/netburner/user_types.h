/*--------------------------------------------------------------------*/
/*
Copyright (c) 2008, Electronic Theatre Controls, Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Electronic Theatre Controls, Inc. nor the names of its
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
/*
For normal C99 or C89 compliant environments the integer types in
acnstdtypes  are a subset of the C99 standards defined in inttypes.h.
For C89 compliant environments these types are inferred and defined.

For special cases when you need to define the types separately, #define
USER_DEFINE_INTTYPES 1 in your user_opt.h and include a user_types.h
header in your platform directory. The following types must be defined:

  int8_t
  uint8_t
  int16_t
  uint16_t
  int32_t
  uint32_t
  intptr_t
  uintptr_t

These two may be necessary if you have 64-bit entities in your system
  int64_t
  uint64_t

The corresponding macros should also be defined to 1 if their types are
defined:
  __int8_t_defined
  __int16_t_defined
  __int32_t_defined
  __int64_t_defined
  __intptr_t_defined

Also you must define macros for printf and scanf conversion specifiers -
these are necessary for portability across systems with differing word
lengths:

  PRId8  PRIi8  PRIu8  PRIx8  PRIX8  
  SCNd8  SCNi8  SCNu8  SCNx8  
  PRId16 PRIi16 PRIu16 PRIx16 PRIX16
  SCNd16 SCNi16 SCNu16 SCNx16
  PRId32 PRIi32 PRIu32 PRIx32 PRIX32
  SCNd32 SCNi32 SCNu32 SCNx32

  PRIdPTR PRIiPTR PRIuPTR PRIxPTR PRIXPTR
  SCNdPTR SCNiPTR SCNuPTR SCNxPTR

And if 64-bit values are used:
  PRId64 PRIi64 PRIu64 PRIx64 PRIX64
  SCNd64 SCNi64 SCNu64 SCNx64

*/

#ifndef USER_TYPES_H_
#define USER_TYPES_H_

#include <includes.h>

#define __int8_t_defined
#define __int16_t_defined
#define __int32_t_defined
#define __intptr_t_defined

#define PRId8 "d"
#define PRIi8 "i"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"

#define SCNd8 "hhd"
#define SCNi8 "hhi"
#define SCNu8 "hhu"
#define SCNx8 "hhx"

#define PRId16 "d"
#define PRIi16 "i"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"

#define SCNd16 "hd"
#define SCNi16 "hi"
#define SCNu16 "hu"
#define SCNx16 "hx"

#define PRId32 "ld"
#define PRIi32 "li"
#define PRIu32 "lu"
#define PRIx32 "lx"
#define PRIX32 "lX"

#define SCNd32 "ld"
#define SCNi32 "li"
#define SCNu32 "lu"
#define SCNx32 "lx"

#define PRIdPTR "ld"
#define PRIiPTR "li"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
#define PRIXPTR "lX"

#define SCNdPTR "ld"
#define SCNiPTR "li"
#define SCNuPTR "lu"
#define SCNxPTR "lx"

#endif /*USER_TYPES_H_*/
