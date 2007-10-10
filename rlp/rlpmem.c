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
#include "marshal.h"
#include <syslog.h>

#ifdef CONFIG_RLP_STATICMEM

/***********************************************************************************************/
/*
Socket memory as static array
access using
rlpInitSockets(), rlpFindSocket(), rlpNewSocket(), rlpFreeSocket()

This currently also suffers from the Shlemeil the Painter problem
*/

#define MAX_RLP_SOCKETS 50
#define MAX_RLP_CHANNELS 100
#define CHANNELS_PERSOCKET 50

static netSocket_t rlpSockets[MAX_RLP_SOCKETS];

void rlpInitChannels(void);

void
rlpInitSockets(void)
{
	netSocket_t *sockp;

	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
		sockp->localPort = NETI_PORT_NONE;
	rlpInitChannels();
}

//find the socket (if any) with matching local port 
netSocket_t *
rlpFindNetSock(port_t localPort)
{
	netSocket_t *sockp;
	
	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
	{
		if(localPort == sockp->localPort)
			return sockp;
	}
	return NULL;
}

netSocket_t *
rlpNewNetSock(void)
{
	netSocket_t *sockp;
	
	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
	{
		if (sockp->localPort == NETI_PORT_NONE)
			return sockp;
	}
	return NULL;
}

void 
rlpFreeNetSock(netSocket_t *sockp)
{
	sockp->localPort = NETI_PORT_NONE;
}

/***********************************************************************************************/
/*

The socket/group/channel API is designed to allow dynamic allocation of
channels within groups and/or a linked list or other more sophisticated
implementation. However, this very simplistic (and inefficient for large
numbers) implementation simply maintains an array of channels which are
associated with sockets and groups only by their content. There is really no
independent rlpChannelGroup_t - channelGroup is simply the first channel in
the array with the correct group and socket association

API
--
void rlpInitChannels(void);
rlpChannelGroup_t *rlpNewChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
void rlpFreeChannelGroup(netSocket_t *netsock, rlpChannelGroup_t *channelgroup)
rlpChannelGroup_t *rlpFindChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
int sockHasGroups(netSocket_t *netsock)

rlpChannel_t *rlpNewChannel(rlpChannelGroup_t *channelgroup)
void rlpFreeChannel(rlpChannelGroup_t *channelgroup, rlpChannel_t *channel)
rlpChannel_t *rlpFirstChannel(rlpChannelGroup_t *channelgroup, acnProtocol_t pduProtocol)
rlpChannel_t *rlpNextChannel(rlpChannelGroup_t *channelgroup, rlpChannel_t *channel, pduProtocol)
int groupHasChannels(rlpChannelGroup_t *channelgroup)

rlpChannelGroup_t *getChannelgroup(rlpChannel_t *channel)
netSocket_t *getNetsock(rlpChannelGroup_t *channelgroup)

*/

typedef struct {
	int socketNum;			// negative for no association
	netAddr_t groupAddr;
	acnProtocol_t protocol;	// PROTO_NONE if this channel within the group is not used
	rlpHandler_t *callback;
	void *ref;
} rlpChannel_t;

static rlpChannel_t rlpChannels[MAX_RLP_CHANNELS];

typedef rlpChannel_t rlpChannelGroup_t;

/*
Initialize channels and groups
*/
void 
rlpInitChannels(void)
{
	rlpChannel_t *channel;

	for (channel = rlpChannels; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel) channel->socketNum = -1;
}

/*
"Create" a new empty channel group and associate it with a socket and groupAddr
*/
rlpChannelGroup_t *
rlpNewChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
{
	rlpChannelGroup_t *channelgroup;

	for (channelgroup = rlpChannels; channelgroup < rlpChannels + MAX_RLP_CHANNELS; ++channelgroup)
	{
		if (channelgroup->socketNum < 0)		// negative socket marks unused channel
		{
			channelgroup->socketNum = netsock - rlpSockets;
			channelgroup->groupAddr = groupAddr;
			channelgroup->protocol = PROTO_NONE;
			return channelgroup;
		}
	}
	return NULL;
}

/*
Free a channel group
Only call if group is empty (no channels exist)
*/
void 
rlpFreeChannelGroup(netSocket_t *netsock, rlpChannelGroup_t *channelgroup)
{
	channelgroup->socketNum = -1;
}

/*
find a channelgroup associated with this socket which has the correct groupAddr
*/
rlpChannelGroup_t *
rlpFindChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
{
	rlpChannelGroup_t *channelgroup;
	int sockx;

	sockx = netsock - rlpSockets;	// index of our socket
	for (channelgroup = rlpChannels; channelgroup < rlpChannels + MAX_RLP_CHANNELS; ++channelgroup)
	{
		if (
			channelgroup->socketNum == sockx
			&& channelgroup->groupAddr == groupAddr
			)
			return channelgroup;	// return first matching channel
	}
	return NULL;
}

/*
"Create" a new empty (no associated protocol) channel within a group
*/
rlpChannel_t *
rlpNewChannel(rlpChannelGroup_t *channelgroup)
{
	rlpChannel_t *channel;

	if (channelgroup->protocol == PROTO_NONE) return channelgroup;	// first is unused
	
	for (channel = channelgroup; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel)
	{
		if (channel->socketNum < 0)		// negative socket marks unused channel
		{
			channel->socketNum = channelgroup->socketNum;
			channel->groupAddr = channelgroup->groupAddr;
			channel->protocol = PROTO_NONE;
			return channel;
		}
	}
	return NULL;
}

/*
Free an unused channel
*/
void 
rlpFreeChannel(rlpChannelGroup_t *channelgroup, rlpChannel_t *channel)
{
	if (channel == channelgroup) channel->protocol = PROTO_NONE;
	else channel->socketNum = -1;
}

/*
Find the first channel in a group with a given protocol
*/
rlpChannel_t *
rlpFirstChannel(rlpChannelGroup_t *channelgroup, acnProtocol_t pduProtocol)
{
	rlpChannel_t *channel;
	int sockx = channelgroup->socketNum;
	netAddr_t groupAddr = channelgroup->groupAddr;
	
	for (channel = channelgroup; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel)
	{
		if (
			channel->socketNum == sockx
			&& channel->groupAddr == groupAddr
			&& channel->protocol == pduProtocol
			)
			return channel;
	}
	return NULL;
}

/*
Find the next channel in a group with a given protocol
*/
#define rlpNextChannel(channelgroup, channel, pduProtocol) rlpFirstChannel((channel) + 1, (pduProtocol))

/*
true if a socket has channelgroups
*/
int 
sockHasGroups(netSocket_t *netsock)
{
	rlpChannel_t *channel;
	int sockx;

	sockx = netsock - rlpSockets;	// index of our socket
	for (channel = rlpChannels; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel)
		if (channel->socketNum == sockx) return 1;
	return 0;
}

/*
true if a channelgroup is not empty
*/
int 
groupHasChannels(rlpChannelGroup_t *channelgroup)
{
	rlpChannel_t *channel;
	int sockx = channelgroup->socketNum;
	netAddr_t groupAddr = channelgroup->groupAddr;

	for (channel = channelgroup; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel)
		if (
			channel->socketNum == sockx
			&& channel->groupAddr == groupAddr
			&& channel->protocol != PROTO_NONE
			)
			return 1;
	return 0;
}

/*
Get the group containing a given channel
*/
#define getChannelgroup(channel) rlpFindChannelGroup(rlpSockets + (channel)->socketNum, (channel)->groupAddr)

/*
Get the netSocket containing a given group
*/
#define getNetsock(channelgroup) (rlpSockets + (channelgroup)->socketNum)

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

#endif

