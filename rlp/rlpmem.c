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
/*
This module is separated from the main RLP to allow customization of memory
handling. See rlp.c for description of 3-level structure.

*/
static const char *rcsid __attribute__ ((unused)) =
   "$Id$";

#include <string.h>
#include "opt.h"
#include "acn_arch.h"

#include "acn_rlp.h"
#include "netiface.h"
#include "rlp.h"
#include "rlpmem.h"
#include "marshal.h"

#if CONFIG_RLPMEM_MALLOC
#include <stdlib.h>
#endif

/***********************************************************************************************/
/*
The socket/group/listener API is designed to allow dynamic allocation of
channels within groups and/or a linked list or other more sophisticated
implementation.

API
--
rlpm_netsocks_init(), rlpFindSocket(), rlpNewSocket(), rlpFreeSocket()
void rlpm_listeners_init(void);
struct rlp_rxgroup_s *rlp_new_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr)
void rlp_free_rxgroup(struct netsocket_s *netsock, struct rlp_rxgroup_s *rxgroup)
struct rlp_rxgroup_s *rlpm_find_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr)
int rlpm_netsock_has_rxgroups(struct netsocket_s *netsock)

struct rlp_listener_s *rlpm_new_listener(struct rlp_rxgroup_s *rxgroup)
void rlpm_free_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener)
struct rlp_listener_s *rlpm_first_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol)
struct rlp_listener_s *rlpNextChannel(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener, pduProtocol)
int rlpm_rxgroup_has_listeners(struct rlp_rxgroup_s *rxgroup)

struct rlp_rxgroup_s *rlpm_get_rxgroup(struct rlp_listener_s *listener)
struct netsocket_s *rlpm_get_netsock(struct rlp_rxgroup_s *rxgroup)
*/
/***********************************************************************************************/


#if CONFIG_RLPMEM_STATIC

/***********************************************************************************************/
/*
socket/group/listener memory as static array

This very simplistic (and inefficient for large numbers) implementation
simply maintains an array of channels which are associated with sockets
and groups only by their content. There is really no independent struct
rlp_rxgroup_s - channelGroup is simply the first listener in the array
with the correct group and socket association

This currently suffers from the Shlemeil the Painter problem
*/

/***********************************************************************************************/

struct netsocket_s sockets[MAX_RLP_SOCKETS];
static struct rlp_listener_s listeners[MAX_LISTENERS];
static struct rlp_txbuf_s txbufs[MAX_TXBUFS];

/***********************************************************************************************/
/*
  find the socket (if any) with matching local address 
*/
struct netsocket_s *
rlpm_find_netsock(localaddr_t localaddr)
{
	struct netsocket_s *sockp;

	sockp = sockets;
#if CONFIG_LOCALIP_ANY
	while (NSK_PORT(*sockp) != LCLAD_PORT(localaddr))
#else
	while (NSK_PORT(*sockp) != LCLAD_PORT(localaddr) || NSK_INADDR(*sockp) != LCLAD_INADDR(localaddr))
#endif
		if (++sockp >= sockets + MAX_RLP_SOCKETS) return NULL;
	return sockp;
}

/***********************************************************************************************/
struct netsocket_s *
rlpm_new_netsock(void)
{
	struct netsocket_s *sockp;

	sockp = sockets;
	while (NSK_PORT(*sockp) != NETI_PORT_NONE)
		if (++sockp >= sockets + MAX_RLP_SOCKETS) return NULL;
	return sockp;
}

/***********************************************************************************************/
void 
rlpm_free_netsock(struct netsocket_s *sockp)
{
	NSK_PORT(*sockp) = NETI_PORT_NONE;
}

/***********************************************************************************************/
void
rlpm_netsocks_init(void)
{
	struct netsocket_s *sockp;

	for (sockp = sockets; sockp < sockets + MAX_RLP_SOCKETS; ++sockp)
		NSK_PORT(*sockp) = NETI_PORT_NONE;
}

/***********************************************************************************************/
struct netsocket_s *
rlpm_next_netsock(struct netsocket_s *sockp)
{
	while (++sockp < sockets + MAX_RLP_SOCKETS)
		if (NSK_PORT(*sockp) != NETI_PORT_NONE) return sockp;
	return NULL;
}

/***********************************************************************************************/
struct netsocket_s *
rlpm_first_netsock(void)
{
	return rlpm_next_netsock(sockets - 1);
}

/***********************************************************************************************/
/*
find a rxgroup associated with this socket which has the correct groupaddr
*/
struct rlp_rxgroup_s *
rlpm_find_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr)
{
	struct rlp_rxgroup_s *rxgroup;
	int sockix;

	sockix = netsock - sockets;	// index of our socket

  // treat all non-multicast the as the same
  if (!is_multicast(groupaddr)) {
    groupaddr = NETI_GROUP_UNICAST;
  }

	for (rxgroup = listeners; rxgroup < listeners + MAX_LISTENERS; ++rxgroup)
	{
		if (
			rxgroup->socketNum == sockix
			&& rxgroup->groupaddr == groupaddr
			)
		{
			return rxgroup;	// return first matching listener
		}
	}
	return NULL;
}

/***********************************************************************************************/
/*
Free a listener group
Only call if group is empty (no listeners exist)
*/
void 
rlpm_free_rxgroup(struct netsocket_s *netsock, struct rlp_rxgroup_s *rxgroup)
{
	UNUSED_ARG(netsock);
	rxgroup->socketNum = -1;
}

/***********************************************************************************************/
/*
"Create" a new empty listener group and associate it with a socket and groupaddr
*/
struct rlp_rxgroup_s *
rlpm_new_rxgroup(struct netsocket_s *netsock, groupaddr_t groupaddr)
{
	struct rlp_rxgroup_s *rxgroup;

	for (rxgroup = listeners; rxgroup < listeners + MAX_LISTENERS; ++rxgroup)
	{
		if (rxgroup->socketNum < 0)		// negative socket marks unused listener
		{
			rxgroup->socketNum = netsock - sockets;
			rxgroup->groupaddr = groupaddr;
			rxgroup->protocol = PROTO_NONE;
			return rxgroup;
		}
	}
	return NULL;
}

/***********************************************************************************************/
/*
"Create" a new empty (no associated protocol) listener within a group
*/
struct rlp_listener_s *
rlpm_new_listener(struct rlp_rxgroup_s *rxgroup)
{
	int sockix;

	if (rxgroup->protocol == PROTO_NONE) return rxgroup;	// first is unused
	
	sockix = rxgroup->socketNum;
	while (++rxgroup < listeners + MAX_LISTENERS)
	{
		if (rxgroup->socketNum < 0)		// negative socket marks unused listener
		{
			rxgroup->socketNum = sockix;
			rxgroup->protocol = PROTO_NONE;
			return rxgroup;
		}
	}
	return NULL;
}

/***********************************************************************************************/
/*
Free an unused listener
*/
void 
rlpm_free_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener)
{
	if (listener == rxgroup) listener->protocol = PROTO_NONE;
	else listener->socketNum = -1;
}

/***********************************************************************************************/
/*
Find the next listener in a group with a given protocol
*/
static
struct rlp_listener_s *
__next_listener(struct rlp_rxgroup_s *rxgroup, int socketNum, groupaddr_t groupaddr, protocolID_t pduProtocol)
{
	while (++rxgroup < listeners + MAX_LISTENERS)
	{
		if (
			rxgroup->socketNum == socketNum
			&& rxgroup->groupaddr == groupaddr
			&& rxgroup->protocol == pduProtocol
			)
			return rxgroup;
	}
	return NULL;
}

/***********************************************************************************************/
/*
Find the next listener in a group with a given protocol
*/
struct rlp_listener_s *
rlpm_next_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener, protocolID_t pduProtocol)
{
	UNUSED_ARG(rxgroup);
	return __next_listener(listener, listener->socketNum, listener->groupaddr, pduProtocol);
}

/***********************************************************************************************/
/*
Find the first listener in a group with a given protocol
*/
struct rlp_listener_s *
rlpm_first_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol)
{
	if (rxgroup->protocol == pduProtocol) return rxgroup;
	return __next_listener(rxgroup, rxgroup->socketNum, rxgroup->groupaddr, pduProtocol);
}

/***********************************************************************************************/
/*
Initialize channels and groups
*/
void 
rlpm_listeners_init(void)
{
	struct rlp_listener_s *listener;

	for (listener = listeners; listener < listeners + MAX_LISTENERS; ++listener) listener->socketNum = -1;
}

/***********************************************************************************************/
/*
true if a socket has channelgroups
*/
int 
rlpm_netsock_has_rxgroups(struct netsocket_s *netsock)
{
	struct rlp_listener_s *listener;
	int sockix;

	sockix = netsock - sockets;	// index of our socket
	for (listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
		if (listener->socketNum == sockix) return 1;
	return 0;
}

/***********************************************************************************************/
/*
true if a rxgroup is not empty
*/
int 
rlpm_rxgroup_has_listeners(struct rlp_rxgroup_s *rxgroup)
{
	int sockix;
	groupaddr_t groupaddr;

	if (rxgroup->protocol != PROTO_NONE) return 1;

	sockix = rxgroup->socketNum;
	groupaddr = rxgroup->groupaddr;
	
	while (++rxgroup < listeners + MAX_LISTENERS)
		if (
			rxgroup->socketNum == sockix
			&& rxgroup->groupaddr == groupaddr
			&& rxgroup->protocol != PROTO_NONE
			)
			return 1;
	return 0;
}

/***********************************************************************************************/
/*
Get the group containing a given listener
*/
#define rlpm_get_rxgroup(listener) rlpm_find_rxgroup(sockets + (listener)->socketNum, (listener)->groupaddr)

/***********************************************************************************************/
/*
Get the netSocket containing a given group
*/
#define rlpm_get_netsock(rxgroup) (sockets + (rxgroup)->socketNum)


/***********************************************************************************************/
void 
rlpm_init(void)
{
	rlpm_netsocks_init();
	rlpm_listeners_init();
}

/***********************************************************************************************/
/*
Transmit network buffer API
(note receive buffers may be the same thing internally but are not externally treated the same)
Network buffers are struct rlp_txbuf_s {...};
Client protocols obtain network buffers using rlpm_newtxbuf() and rlpm_freetxbuf()
The can obtain a pointer to the data area of the buffer using a macro:

  rlpItsData(buf)

When calling RLP to transmit data, they pass both the pointer to the buffer and a pointer to the 
data to send which need not start at the beginning of the buffers data area.
This allows for example, a higher protocol to re-transmit only a part of a former packet.
However, in this case, the content of the data area before that passed may be overwritten by rlp.

To track usage the protocol can use rlp_incuse(buf) and rlp_decuse(buf). Also rlp_getuse(buf)
which will be non zero if the buffer is used. These can generally be implemented as macros.

rlpm_freetxbuf will only actually free the buffer if usage is zero.

*/
/***********************************************************************************************/
static int bufnum = 0;

struct 
rlp_txbuf_s *rlpm_newtxbuf(int size, component_t *owner)
{
	int i;
	UNUSED_ARG(size);
	
	i = bufnum;
	while (txbufs[i].usage != 0)
	{
		++i;
		if (i >= MAX_TXBUFS) i = 0;
		if (i == bufnum) return NULL;
	}

	txbufs[i].usage = 1;
	txbufs[i].owner = owner;
	bufnum = i;
	return txbufs + i;
}

/***********************************************************************************************/
void 
rlpm_freetxbuf(struct rlp_txbuf_s *buf)
{
	--(buf->usage);
}

#define rlpItsData(buf) ((uint8_t *)(buf) \
			+ sizeof(struct rlpTxbufhdr_s) \
			+ RLP_PREAMBLE_LENGTH \
			+ sizeof(protocolID_t) \
			+ sizeof(cid_t))

#define rlp_getuse(buf) (((struct rlpTxbufhdr_s *)(buf))->usage)
#define rlp_incuse(buf) (++rlp_getuse(buf))
#define rlp_decuse(buf) (--rlp_getuse(buf))

/***********************************************************************************************/

#elif CONFIG_RLPMEM_MALLOC

/***********************************************************************************************/
struct rlp_txbuf_s *rlpm_newtxbuf(int size, component_t *owner)
{
	uint8_t *buf;
	
	buf = malloc(
			sizeof(struct rlp_txbuf_s) 
			+ RLP_PREAMBLE_LENGTH
			+ sizeof(protocolID_t)
			+ sizeof(cid_t)
			+ size
			+ RLP_POSTAMBLE_LENGTH
		);
	if (buf != NULL)
	{
		rlp_getuse(buf) = 0;
		((struct rlp_txbuf_s *)buf)->datasize = RLP_PREAMBLE_LENGTH
				+ sizeof(protocolID_t)
				+ sizeof(cid_t)
				+ size
				+ RLP_POSTAMBLE_LENGTH;
		((struct rlp_txbuf_s *)buf)->owner = owner;
	}

	return buf;
}

/***********************************************************************************************/
void rlpm_freetxbuf(struct rlp_txbuf_s *buf)
{
	if (!rlp_getuse(buf)) free(buf);
}

#define rlpItsData(buf) ((uint8_t *)(buf) \
			+ sizeof(struct rlpTxbufhdr_s) \
			+ RLP_PREAMBLE_LENGTH \
			+ sizeof(protocolID_t) \
			+ sizeof(cid_t))

#define rlp_getuse(buf) (((struct rlpTxbufhdr_s *)(buf))->usage)
#define rlp_incuse(buf) (++rlp_getuse(buf))
#define rlp_decuse(buf) (--rlp_getuse(buf))

/***********************************************************************************************/
/*

*/

#endif	/* #elif CONFIG_RLPMEM_MALLOC */

