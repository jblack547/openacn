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
/*--------------------------------------------------------------------*/

#ifndef __acn_arch_h__
#define __acn_arch_h__ 1

#include "types.h"

typedef uint32_t protocolID_t;
#define PROTO_NONE 0

#include "uuid.h"

typedef uuid_t cid_t;

/* PDU flags */
/* flag and length field is 16 bits */
#define LENGTH_FLAG    0x8000
#define VECTOR_FLAG    0x4000
#define HEADER_FLAG    0x2000
#define DATA_FLAG      0x1000
#define LENGTH_MASK    0x0FFF

/* sometimes we only want 8 bits */
#define LENGTH_bFLAG    0x80
#define VECTOR_bFLAG    0x40
#define HEADER_bFLAG    0x20
#define DATA_bFLAG      0x10
#define LENGTH_bMASK    0x0F

#include "marshal.h"
#define getpdulen(pdup) (unmarshalU16(pdup) & 0x0fff)

#endif
