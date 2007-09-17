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

	$Id: pdu.h 16 2007-09-07 09:52:07Z philip $

*/
/*--------------------------------------------------------------------*/
#ifndef __pdu_h__
#define __pdu_h__

#include <arch/types.h>


typedef struct
{
   uint32 data[4];
} uuid_type;

//flag field is 16 bits
#define LENGTH_FLAG    0x8000
#define VECTOR_FLAG    0x4000
#define HEADER_FLAG    0x2000
#define DATA_FLAG      0x1000
#define LENGTH_FIELD   0x0FFF

#define vectorPresent(x) (VECTOR_FLAG & ((uint16)(x)))
#define headerPresent(x) (HEADER_FLAG & ((uint16)(x)))
#define dataPresent(x) (DATA_FLAG & ((uint16)(x)))

typedef struct
{
   uint8 *header;
   uint8 *data;
	uint8 *location;
	uint32 vector;
   uint16 length;	
	uint8 headerLength;
	uint8 vectorLength;
	uint16 dataLength;
	uint16 flags;	
	
} pdu_header_type;

typedef struct
{
	uint32 packetStart;
	uint32 sdtStart;
	uint32 dmpStart;
	uint32 dmpEnd;
	uint32 sdtEnd;
	uint32 packetEnd;
	uint32 numberofdmpprops;
} timing_t;
extern timing_t benchmark;


enum 
{
   PROTO_DMP = 2,
   PROTO_SDT = 1, 
};

enum
{
   ACN_ADDRESS_NOT_PRESENT = 0,
   ACN_ADDRESS_16_BIT = 1,
   ACN_ADDRESS_128_BIT = 2,
   ACN_ADDRESS_ALL = 3,
};

#define BROADCAST_MID 0xFFFF

int calculatePduLength(pdu_header_type *header);
int decodePdu(uint8 *buf, pdu_header_type *pdu, int vectorLen, int headerLen);
int formatPdu(uint8 *buf, pdu_header_type *pdu);
void updatePduLen(pdu_header_type *pdu);
#endif
