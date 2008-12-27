/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Pathway Connectivity Inc.
Copyright (c) 2008, Pathway Electronic Theatre Controls, Inc.


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
#ifndef __byteorder_h__
#define __byteorder_h__

/************************************************************************/
/*
  These are probably defined in standard system headers (e.g. netinet/in.h)
  If they are not this deals with it

  Coldfire architecture is big endian
*/

#if !defined(BYTE_ORDER)
#define BYTE_ORDER BIG_ENDIAN
#endif

#if !defined(LITTLE_ENDIAN)
#define LITTLE_ENDIAN 1234
#endif

#if !defined(BIG_ENDIAN)
#define BIG_ENDIAN    4321
#endif

/************************************************************************/
/*
 There are likely to be (highly optimized) versions of these already.
*/

#if !defined(__bswap_16)
#define __bswap_16(x) \
      	( (((x) & 0xff00) >> 8) \
      	| (((x) & 0x00ff) << 8) )
#endif

#if !defined(__bswap_32)
#define __bswap_32(x) \
      	( (((x) & 0xff000000) >> 24) \
      	| (((x) & 0x00ff0000) >> 8) \
      	| (((x) & 0x0000ff00) << 8) \
      	| (((x) & 0x000000ff) << 24) )
#endif


/************************************************************************/
/*
 Network to native conversions are likewise defined in one of the standard
 includes for your stack or system e.g. <netinet/in.h>
*/
#if !defined(HAVE_ntohl)
/*originals do not work intended */
/* #define ntohl(x) __bswap_32(x) */
#define ntohl(x) (x)
#endif

#if !defined(HAVE_ntohs)
/*originals do not work intended */
/* #define ntohs(x) __bswap_16(x) */
#define ntohs(x) (x)
#endif

#if !defined(HAVE_htonl)
/*originals do not work intended */
/* #define htonl(x) __bswap_32(x) */
#define htonl(x) (x)
#endif

#if !defined(HAVE_htons)
/*originals do not work intended */
/* #define htons(x) __bswap_16(x) */
#define htons(x) (x)
#endif

/*  Improved calls (if needed)
  if ntohl() is use like this:
    x = ntohl(foo(x));
  then, with the version below, foo(x) will only be called once and not
  re-evaluated on each call.
*/
static __inline unsigned long int acn_ntohl(register unsigned long int x)
{
  register unsigned long int __x = x;
  return __bswap_32(__x);
}

static __inline unsigned short acn_ntohs(register unsigned short int x)
{
  register unsigned short int __x = x;
  return __bswap_16(__x);
}

static __inline unsigned long int acn_htonl(register unsigned long int x)
{
  register unsigned long int __x = x;
  return __bswap_32(__x);
}

static __inline unsigned short acn_htons(register unsigned short int x)
{
  register unsigned short int __x = x;
  return __bswap_16(__x);
}

#endif /* __byteorder_h__ */
