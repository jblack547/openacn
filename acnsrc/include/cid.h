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

#tabs=3s
*/
/*--------------------------------------------------------------------*/
/* Universally Unique Identifier (UUID). RFC 4122 */

#ifndef __cid_h__
#define __cid_h__ 1

#include <stdio.h>
#include "opt.h"
#include "acnstdtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
For most purposes a CID is simply an array of 16 octets
*/
#define CIDSIZE 16
#define CID_STR_SIZE 37  /* including null termination */
#define PRICIDstr "36"	/* printing width for string format */

/* generic cid as array */
typedef uint8_t cid_t[CIDSIZE];

extern const cid_t  null_cid;

/*
  Macros to access the internal structure
  Fields are in Network byte order
*/
#define CID_TIME_LOW(cid) ntohl(*(uint32_t *)(cid))
#define CID_TIME_MID(cid) ntohs(*(uint16_t *)((cid) + 4))
#define CID_TIME_HIV(cid) ntohs(*(uint16_t *)((cid) + 6))
#define CID_CLKSEQ_HI(cid) (*((cid) + 8))
#define CID_CLKSEQ_LOW(cid) (*((cid) + 9))
#define CID_NODE(cid) ((cid) + 10)

int textToCid(const char *cidText, cid_t cidp);
char *cidToText(const cid_t cidp, char *cidText);

/* Macro version of functions */
#define cidIsEqual(cid1, cid2) (memcmp(cid1, cid2, sizeof(cid_t)) == 0)
#define cidNull(cid) (memset(cid, 0, sizeof(cid_t)))
#define cidIsNull(cid) cidIsNull(cid)
#define cidCopy(dst, src) (memcpy(dst, src, sizeof(cid_t)))

static inline int cidIsNull(cid_t cid)
{
   int i = CIDSIZE;

   while (i--) if (*cid++) return 0;
   return 1;
}

#if !defined(cidIsEqual)
int cidIsEqual(const cid_t cid1, const cid_t cid2);
#endif
#if !defined(cidNull)
void cidNull(cid_t cid);
#endif
/*
#if !defined(cidIsNull)
int  cidIsNull(const cid_t cid);
#endif
*/
#if !defined(cidCopy)
void cidCopy(cid_t cid1, const cid_t cid2);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __cid_h__ */
