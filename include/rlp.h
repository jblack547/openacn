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

	$Id: rlp.h 16 2007-09-07 09:52:07Z philip $

*/
/*--------------------------------------------------------------------*/
#ifndef __rlp_h__
#define __rlp_h__

#include <arch/types.h>

#define ACN_RESERVED_PORT	5568
#define NUM_PACKET_BUFFERS  16 //This must be a power of 2 or rollover breaks
#define LOCAL_ADHOC_PORT	 ACN_RESERVED_PORT
typedef struct
{
	uint32 srcIP;
	uint32 dstIP;
	uint16 srcPort;
	uint16 dstPort;
} udp_transport_t;

int rlpSendTo(void *sock, uint32 dstIP, uint16 dstPort, int keep);
void *rlpOpenSocket(uint16 localPort);
void rlpCloseSocket(void *s);
void *rlpGetSocket(uint16 localPort);
void initRlp(void);
uint8 *rlpFormatPacket(const uint8 *srcCid, int vector);

int rlpEnqueue(int length);
void rlpResendTo(void *sock, uint32 dstIP, uint16 dstPort, int numBack);
#endif
