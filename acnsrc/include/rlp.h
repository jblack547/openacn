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
#define __rlp_h__ 1

#include "netxface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void rlpHandler_t(
	const uint8_t *data,
	int datasize,
	void *ref,
	const netx_addr_t *remhost,
	const cid_t remcid
);

/* must be included after rlpHandler_t */
#include "rlpmem.h"

/* Prototypes */
int       rlp_init(void);
uint8_t  *rlp_init_block(struct rlp_txbuf_s *buf, uint8_t *datap);
#if !CONFIG_RLP_SINGLE_CLIENT
uint8_t  *rlp_add_pdu(struct rlp_txbuf_s *buf, uint8_t *pdudata, int size, protocolID_t protocol, uint8_t **packetdatap);
#else
uint8_t  *rlp_add_pdu(struct rlp_txbuf_s *buf, uint8_t *pdudata, int size, uint8_t **packetdatap);
#endif
int       rlp_send_block(struct rlp_txbuf_s *buf, netxsocket_t *netsock, const netx_addr_t *destaddr);
netxsocket_t * rlp_open_netsocket(localaddr_t *localaddr);
void      rlp_close_netsocket(netxsocket_t *netsock);
struct    rlp_listener_s * rlp_add_listener(netxsocket_t *netsock, groupaddr_t groupaddr, protocolID_t protocol, rlpHandler_t *callback, void *ref);
void      rlp_del_listener(netxsocket_t *netsock, struct rlp_listener_s *listener);
/* void      rlp_process_packet(netxsocket_t *netsock, const uint8_t *data, int dataLen, ip4addr_t destaddr, const netx_addr_t *remhost, void *ref); */
void      rlp_process_packet(netxsocket_t *socket, const uint8_t *data, int length, netx_addr_t *dest, netx_addr_t *source, void *ref);


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

#define rlp_getuse(buf) (((struct rlp_txbuf_s *)(buf))->usage)
#define rlp_incuse(buf) (++rlp_getuse(buf))
#define rlp_decuse(buf) if (rlp_getuse(buf)) --rlp_getuse(buf)

#ifdef __cplusplus
}
#endif

#endif
