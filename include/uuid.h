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
/* Universally Unique Identifier (UUID). RFC 4122 */

#ifndef __uuid_h__
#define __uuid_h__ 1

#include <stdio.h>
#include "opt.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
For most purposes a UUID is simply an array of 16 octets
*/
#define UUIDSIZE 16
#define UUID_STR_SIZE 37  /* includeing null termination */

/* msvc++ has this defined so we need to undefine it */
#ifdef uuid_t
#undef uuid_t
#endif

/* generic uuid as array */
typedef uint8_t uuid_t[UUIDSIZE];

/* same by another name */
typedef uuid_t cid_t;

static const uuid_t  null_cid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
  Macros to access the internal structure
  Fields are in Network byte order
*/
#define UUID_TIME_LOW(uuid) ntohl(*(uint32_t *)(uuid))
#define UUID_TIME_MID(uuid) ntohs(*(uint16_t *)((uuid) + 4))
#define UUID_TIME_HIV(uuid) ntohs(*(uint16_t *)((uuid) + 6))
#define UUID_CLKSEQ_HI(uuid) (*((uuid) + 8))
#define UUID_CLKSEQ_LOW(uuid) (*((uuid) + 9))
#define UUID_NODE(uuid) ((uuid) + 10)

int textToUuid(const char *uuidText, uuid_t uuidp);
char *uuidToText(const uuid_t uuidp, char *uuidText);

/* Macro version of functions */
#define uuidIsEqual(uuid1, uuid2) (memcmp(uuid1, uuid2, sizeof(uuid_t)) == 0)
#define uuidNull(uuid) (memset(uuid, 0, sizeof(uuid_t)))
#define uuidIsNull(uuid) (memcmp(uuid, &null_cid, sizeof(uuid_t)) == 0)
#define uuidCopy(uuid1, uuid2) (memcpy(uuid1, uuid2, sizeof(uuid_t)))

#if !defined(uuidIsEqual)
int uuidIsEqual(const uuid_t uuid1, const uuid_t uuid2);
#endif
#if !defined(uuidNull)
void uuidNull(uuid_t uuid);
#endif
#if !defined(uuidIsNull)
int  uuidIsNull(const uuid_t uuid);
#endif
#if !defined(uuidCopy)
void uuidCopy(uuid_t uuid1, const uuid_t uuid2);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __uuid_h__ */
