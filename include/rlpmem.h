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
#include "component.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef int usage_t;

#define RLP_FAIL 0
#define RLP_OK   1

typedef  struct rlp_listener_s rlp_listener_t;
typedef  struct rlp_rxgroup_s  rlp_rxgroup_t;
typedef  struct rlp_txbuf_s    rlp_txbuf_t;

struct rlp_listener_s {
  struct rlp_rxgroup_s  *rxgroup;    /* group this listener references */
	protocolID_t           protocol;	 /* PROTO_NONE if this listener within the group is not used */
	rlpHandler_t          *callback;   /* callback when data is available*/
  struct rlp_listener_s *next;       /* next listener in group list*/
	void                  *ref;        /* reference */
};

struct rlp_rxgroup_s {
  netxsocket_t          *socket;     /* NULL for no association */
  ip4addr_t              groupaddr;  /* multicast address */
  struct rlp_listener_s *listeners;  /* first listener in group*/
  struct rlp_rxgroup_s  *next;       /* next receive group */
};

struct rlp_txbuf_s {
	usage_t             usage;         /* usage counter */
	unsigned int        datasize;      /* size of data block */
	uint8_t            *blockstart;
	uint8_t            *blockend;
	protocolID_t        protocol;      /* protocol of data */
	component_t        *owner;         /* component that registered the callback */
  void               *netbuf;        /* pointer to network buffer */
  uint8_t            *datap;         /* pointer to data inside of network buffer*/
};

extern void rlpm_init(void);
/* extern void rlpm_netsocks_init(void); */
/* extern void rlpm_listeners_init(void); */

/* extern netxsocket_t *rlpm_new_netsock(void); */
/* extern netxsocket_t *rlpm_find_netsock(localaddr_t localaddr); */
/* extern netxsocket_t *rlpm_next_netsock(netxsocket_t *sockp); */
/* extern netxsocket_t *rlpm_first_netsock(void); */
/* extern void rlpm_free_netsock(netxsocket_t *sockp); */

extern rlp_listener_t *rlpm_new_listener(rlp_rxgroup_t *rxgroup);
extern void            rlpm_free_listener(rlp_listener_t *listener);
extern rlp_listener_t *rlpm_first_listener(rlp_rxgroup_t *rxgroup, protocolID_t pdu_protocol);
extern rlp_listener_t *rlpm_next_listener(rlp_listener_t *listener, protocolID_t pduProtocol);

extern rlp_rxgroup_t *rlpm_new_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr);
extern rlp_rxgroup_t *rlpm_find_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr);
extern void           rlpm_free_rxgroup(netxsocket_t *netsock, rlp_rxgroup_t *rxgroup);

extern int            rlpm_netsock_has_rxgroups(netxsocket_t *netsock);
extern int            rlpm_rxgroup_has_listeners(rlp_rxgroup_t *rxgroup);
extern rlp_rxgroup_t *rlpm_get_rxgroup(rlp_listener_t *listener);
extern netxsocket_t  *rlpm_get_netsock(rlp_rxgroup_t *rxgroup);

extern rlp_txbuf_t   *rlpm_new_txbuf(int size, component_t *owner);
extern void           rlpm_free_txbuf(rlp_txbuf_t *txbuf);
extern void           rlpm_release_txbuf(rlp_txbuf_t *txbuf);

#ifdef __cplusplus
}
#endif


#endif  /* __rlpmem_h__ */
