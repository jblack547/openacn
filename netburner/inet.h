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

  Description:
    Header file for ntoa.c
*/
#ifndef __INET_H__
#define __INET_H__

#include "opt.h"
#include "types.h"
#include "acn_arch.h"
#include "ntoa.h"

#include "ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_STACK_NETBURNER

/* ascii to network */
uint32_t inet_aton(const char *cp);

/* network to ascii */
//char *   inet_ntoa(uint32_t n);
#define inet_ntoa(x) ntoa(x)

#ifdef htons
#undef htons
#endif /* htons */

#ifdef htonl
#undef htonl
#endif /* htonl */

#ifdef ntohs
#undef ntohs
#endif /* ntohs */

#ifdef ntohl
#undef ntohl
#endif /* ntohl */


#if BYTE_ORDER == BIG_ENDIAN
 #define htons(x) (x)
 #define ntohs(x) (x)
 #define htonl(x) (x)
 #define ntohl(x) (x)
#else /* BYTE_ORDER != BIG_ENDIAN */
 #error !BIG_INDIAN NOT SUPPORTED YET
#endif /* BYTE_ORDER == BIG_ENDIAN */

#endif /* CONFIG_STACK_NETBURNER */

#ifdef __cplusplus
}
#endif

#endif /* __INET_H__ */
