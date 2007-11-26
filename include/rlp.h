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
#ifndef __rlp_h__
#define __rlp_h__

#include "types.h"

typedef void rlpHandler_t(
	const uint8_t *data,
	int datasize,
	void *ref,
	const netiHost_t *remhost,
	const cid_t *remcid
	);

int rlpSendTo(void *sock, uint32_t dstIP, uint16_t dstPort, int keep);
void *rlpOpenSocket(uint16_t localPort);
void rlpCloseSocket(void *s);
void *rlpFindSocket(uint16_t localPort);
int initRlp(void);
uint8_t *rlpFormatPacket(const uint8_t *srcCid, int vector);
void rlp_process_packet(struct netsocket_s *netsock, const uint8_t *data, int dataLen, ip4addr_t destaddr, const netiHost_t *remhost);

/*
struct rlp_txbuf_hdr {
	usage_t usage;
	unsigned int datasize;
	uint8_t *blockstart;
	uint8_t *blockend;
#if !CONFIG_RLP_SINGLE_CLIENT	
	protocolID_t protocol;
#endif
#if CONFIG_RLP_OPTIMIZE_PACK
	uint8_t *curdata;
	unsigned int curdatalen;
#endif
};
*/

#endif
