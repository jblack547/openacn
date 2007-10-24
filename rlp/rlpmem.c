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
   "$Id: rlp.c 8 2007-10-08 16:32:56Z philipnye $";

#include <string.h>
#include "configure.h"
#include "arch/types.h"
#include "acn_arch.h"
#include "acn_rlp.h"
#include "netiface.h"
#include "rlp.h"
#include "rlpmem.h"
#include "marshal.h"
#include <syslog.h>

/***********************************************************************************************/
/*
The socket/group/listener API is designed to allow dynamic allocation of
channels within groups and/or a linked list or other more sophisticated
implementation.

API
--
rlpm_rlpsocks_init(), rlpFindSocket(), rlpNewSocket(), rlpFreeSocket()
void rlpm_listeners_init(void);
struct rlp_rxgroup_s *rlp_new_rxgroup(struct rlpsocket_s *rlpsock, groupaddr_t groupaddr)
void rlp_free_rxgroup(struct rlpsocket_s *rlpsock, struct rlp_rxgroup_s *rxgroup)
struct rlp_rxgroup_s *rlpm_find_rxgroup(struct rlpsocket_s *rlpsock, groupaddr_t groupaddr)
int rlpm_rlpsock_has_rxgroups(struct rlpsocket_s *rlpsock)

struct rlp_listener_s *rlpm_new_listener(struct rlp_rxgroup_s *rxgroup)
void rlpm_free_listener(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener)
struct rlp_listener_s *rlpm_first_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol)
struct rlp_listener_s *rlpNextChannel(struct rlp_rxgroup_s *rxgroup, struct rlp_listener_s *listener, pduProtocol)
int rlpm_rxgroup_has_listeners(struct rlp_rxgroup_s *rxgroup)

struct rlp_rxgroup_s *rlpm_get_rxgroup(struct rlp_listener_s *listener)
struct rlpsocket_s *rlpm_get_rlpsock(struct rlp_rxgroup_s *rxgroup)
*/
/***********************************************************************************************/


#ifdef CONFIG_RLPMEM_STATIC

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

#define MAX_RLP_SOCKETS 50
#define MAX_LISTENERS 100
#define CHANNELS_PERSOCKET 50

static struct rlpsocket_s sockets[MAX_RLP_SOCKETS];

static struct rlp_listener_s listeners[MAX_LISTENERS];

typedef struct rlp_listener_s struct rlp_rxgroup_s;

void rlpm_listeners_init(void);

/***********************************************************************************************/
/*
  find the socket (if any) with matching port 
*/
static inline
struct rlpsocket_s *
_find_rlpsock(port_t port)
{
	struct rlpsocket_s *sockp;

	sockp = sockets;
	while (sockp->localport != port)
		if (++sockp >= sockets + MAX_RLP_SOCKETS) return NULL;
	return sockp;
}

/***********************************************************************************************/
struct rlpsocket_s *
rlpm_find_rlpsock(struct netaddr_s *localaddr)
{
	return _find_rlpsock(localaddr->port);
}

/***********************************************************************************************/
struct rlpsocket_s *
rlpm_new_rlpsock(void)
{
	return _find_rlpsock(NETI_PORT_NONE);
}

/***********************************************************************************************/
void 
rlpm_free_rlpsock(struct rlpsocket_s *sockp)
{
	sockp->localport = NETI_PORT_NONE;
}

/***********************************************************************************************/
void
rlpm_rlpsocks_init(void)
{
	struct rlpsocket_s *sockp;

	for (sockp = sockets; sockp < sockets + MAX_RLP_SOCKETS; ++sockp)
		sockp->localport = NETI_PORT_NONE;
	rlpm_listeners_init();
}

/***********************************************************************************************/
/*
find a rxgroup associated with this socket which has the correct groupaddr
*/
struct rlp_rxgroup_s *
rlpm_find_rxgroup(struct rlpsocket_s *rlpsock, groupaddr_t groupaddr)
{
	struct rlp_rxgroup_s *rxgroup;
	int sockix;

	sockix = rlpsock - sockets;	// index of our socket

	for (rxgroup = listeners; rxgroup < listeners + MAX_LISTENERS; ++rxgroup)
	{
		if (
			rxgroup->socketNum == sockix
			&& rxgroup->groupaddr == groupaddr
			)
			return rxgroup;	// return first matching listener
	}
	return NULL;
}

/***********************************************************************************************/
/*
Free a listener group
Only call if group is empty (no listeners exist)
*/
void 
rlpm_free_rxgroup(struct rlpsocket_s *rlpsock, struct rlp_rxgroup_s *rxgroup)
{
	rxgroup->socketNum = -1;
}

/***********************************************************************************************/
/*
"Create" a new empty listener group and associate it with a socket and groupaddr
*/
struct rlp_rxgroup_s *
rlpm_new_rxgroup(struct rlpsocket_s *rlpsock, groupaddr_t groupaddr)
{
	struct rlp_rxgroup_s *rxgroup;

	for (rxgroup = listeners; rxgroup < listeners + MAX_LISTENERS; ++rxgroup)
	{
		if (rxgroup->socketNum < 0)		// negative socket marks unused listener
		{
			rxgroup->socketNum = rlpsock - sockets;
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
rlp_next_listener(struct rlp_rxgroup_s *rxgroup, protocolID_t pduProtocol)
{
	return __next_listener(rxgroup, rxgroup->socketNum, rxgroup->groupaddr, pduProtocol);
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
rlpm_rlpsock_has_rxgroups(struct rlpsocket_s *rlpsock)
{
	struct rlp_listener_s *listener;
	int sockix;

	sockix = rlpsock - sockets;	// index of our socket
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
#define rlpm_get_rlpsock(rxgroup) (sockets + (rxgroup)->socketNum)

#endif

/***********************************************************************************************/
void 
rlpInitBuffers(void)
{
	int i;
	for (i = 0; i < NUM_PACKET_BUFFERS; i++)
	{
		memcpy(buffers[i], rlpPreamble, PREAMBLE_LENGTH);
		bufferLengths[i] = PREAMBLE_LENGTH;
	}
	currentBufNum = 0;
}

uint8_t *
rlpNewPacketBuffer(void)
{
	return buffers[currentBufNum];
}

/***********************************************************************************************/
/*
Transmit network buffer API
(note receive buffers may be the same thing internally but are not externally treated the same)
Network buffers are struct rlpTxBuf {...};
Client protocols obtain network buffers using rlpNewTxBuf() and rlpFreeTxBuf()
The can obtain a pointer to the data area of the buffer using a macro:

  rlpItsData(buf)

When calling RLP to transmit data, they pass both the pointer to the buffer and a pointer to the 
data to send which need not start at the beginning of the buffers data area.
This allows for example, a higher protocol to re-transmit only a part of a former packet.
However, in this case, the content of the data area before that passed may be overwritten by rlp.

To track usage the protocol can use rlpIncUsage(buf) and rlpDecUsage(buf). Also rlpGetUsage(buf)
which will be non zero if the buffer is used. These can generally be implemented as macros.

rlpFreeTxBuf will only actually free the buffer if usage is zero.

*/

/***********************************************************************************************/
struct rlpTxBuf *rlpNewTxBuf(int size, cid_t owner)
{
	uint8_t *buf;
	
	buf = malloc(
			sizeof(struct rlpTxbufhdr_s)
			+ RLP_PREAMBLE_LENGTH
			+ sizeof(protocolID_t)
			+ sizeof(cid_t)
			+ size
			+ RLP_POSTAMBLE_LENGTH
		);
	if (buf != NULL)
	{
		rlpGetUsage(buf) = 0;
		((struct rlpTxbufhdr_s *)buf)->datasize = RLP_PREAMBLE_LENGTH
				+ sizeof(protocolID_t)
				+ sizeof(cid_t)
				+ size
				+ RLP_POSTAMBLE_LENGTH;
		((struct rlpTxbufhdr_s *)buf)->ownerCID = owner;
	}

	return buf;
}

/***********************************************************************************************/
void rlpFreeTxBuf(struct rlpTxBuf *buf)
{
	if (!rlpGetUsage(buf)) free(buf);
}

#define rlpItsData(buf) ((uint8_t *)(buf) \
			+ sizeof(struct rlpTxbufhdr_s) \
			+ RLP_PREAMBLE_LENGTH \
			+ sizeof(protocolID_t) \
			+ sizeof(cid_t))

#define rlpGetUsage(buf) (((struct rlpTxbufhdr_s *)(buf))->usage)
#define rlpIncUsage(buf) (++rlpGetUsage(buf))
#define rlpDecUsage(buf) (--rlpGetUsage(buf))

/***********************************************************************************************/
/*

*/





#elif defined(CONFIG_RLPMEM_DYNAMIC)

#endif	/* #elif defined(CONFIG_RLPMEM_DYNAMIC) */

