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

#ifndef __rlpmem_h__
#define __rlpmem_h__ 1


typedef int usage_t;

#if CONFIG_RLPMEM_STATIC

#define MAX_RLP_SOCKETS 50
#define MAX_LISTENERS 100
#define MAX_TXBUFS 10

struct rlp_listener_s {
	int socketNum;			// negative for no association
	ip4addr_t groupaddr;
	protocolID_t protocol;	// PROTO_NONE if this listener within the group is not used
	rlpHandler_t *callback;
	void *ref;
};

#define rlp_rxgroup_s rlp_listener_s

struct rlp_txbuf_s {
	usage_t usage;
	unsigned int datasize;
	uint8_t *blockstart;
	uint8_t *blockend;
	protocolID_t protocol;
	cid_t ownerCID;
	uint8_t data[MAX_MTU];
};

#endif


#if CONFIG_RLPMEM_MALLOC

struct rlp_txbuf_s;

struct rlp_txbuf_s {
	struct rlp_txbuf_s *next;
	usage_t usage;
	unsigned int datasize;
	uint8_t *blockstart;
	uint8_t *blockend;
	protocolID_t protocol;
	uint8_t data[];
};

#endif

void rlpmem_init(void);

struct rlpsocket_s *rlpm_find_rlpsock(struct netaddr_s *localaddr);
struct rlpsocket_s *rlpm_new_rlpsock(void);
void rlpm_free_rlpsock(struct rlpsocket_s *sockp);

struct rlp_listener_s *rlpm_new_listener(struct rlp_rxgroup_s *rxgroup);
void  rlpm_free_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener);
struct rlp_listener_s *rlp_next_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol);
struct rlp_listener_s *rlpm_first_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol);


#define bufhdrp(buf) ((struct rlp_txbuf_s *)(buf))
#define bufdatap(buf) (((struct rlp_txbuf_s *)(buf))->data)

#endif  /* __rlpmem_h__ */
