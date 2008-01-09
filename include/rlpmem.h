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

#ifndef __rlpmem_h__
#define __rlpmem_h__ 1

#include "opt.h"
#include "acn_arch.h"

typedef int usage_t;

#if CONFIG_RLPMEM_STATIC

extern struct netsocket_s sockets[MAX_RLP_SOCKETS];

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

extern void rlpmem_init(void);
extern void rlpm_netsocks_init(void);
extern void rlpm_listeners_init(void);

extern struct netsocket_s *rlpm_new_netsock(void);
extern struct netsocket_s *rlpm_find_netsock(localaddr_t localaddr);
extern void rlpm_free_netsock(struct netsocket_s *sockp);

extern struct rlp_listener_s *rlpm_new_listener(struct rlp_rxgroup_s *rxgroup);
extern void  rlpm_free_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener);
extern struct rlp_listener_s *rlpm_first_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol);
extern struct rlp_listener_s *rlpm_next_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener, protocolID_t pduProtocol);

extern struct rlp_rxgroup_s *rlpm_new_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr);
extern struct rlp_rxgroup_s *rlpm_find_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr);
extern void  rlpm_free_rxgroup(struct netsocket_s *netsock, struct rlp_rxgroup_s *rxgroup);

extern int rlpm_netsock_has_rxgroups(struct netsocket_s *netsock);
extern int rlpm_rxgroup_has_listeners(struct rlp_rxgroup_s *rxgroup);
extern struct rlp_rxgroup_s *rlpm_get_rxgroup(struct rlp_listener_s *listener);
extern struct netsocket_s *rlpm_get_netsock(struct rlp_rxgroup_s *rxgroup);

extern struct rlp_txbuf_s *rlpm_newtxbuf(int size, cid_t owner);
extern void rlp_freetxbuf(struct rlp_txbuf_s *buf);

#define bufhdrp(buf) ((struct rlp_txbuf_s *)(buf))
#define bufdatap(buf) (((struct rlp_txbuf_s *)(buf))->data)

#if CONFIG_RLPMEM_STATIC
#define rlpm_get_rxgroup(listener) rlpm_find_rxgroup(sockets + (listener)->socketNum, (listener)->groupaddr)
#define rlpm_get_netsock(rxgroup) (sockets + (rxgroup)->socketNum)
#endif


#endif  /* __rlpmem_h__ */
