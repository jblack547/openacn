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
static const char *rcsid __attribute__ ((unused)) =
   "$Id$";
//syslog facility LOG_LOCAL0 is used for ACN:RLP

#define MAX_PACKET_SIZE 1450
#define PREAMBLE_LENGTH 16

//#define __WATTCP_KERNEL__ // for access to the tcpHeader
//#include <wattcp.h>
#include <string.h>
#include "configure.h"
#include "arch/types.h"
#include "acn_arch.h"
#include "netiface.h"
#include "rlp.h"
#include "uuid.h"
#include "pdu.h"
#include "sdt.h"
#include "marshal.h"
#include <syslog.h>

#define BUFFER_ROLLOVER_MASK  (NUM_PACKET_BUFFERS - 1)

#ifdef CONFIG_EPI17
static const struct PACKED rlpPreamble_s rlpPreamble =
{
	htons(RLP_PREAMBLE_LENGTH),
	htons(RLP_POSTAMBLE_LENGTH),
	RLP_SIGNATURE_STRING
};
#endif

/*
Min length structure for first PDU (no fields inherited) is:
	flags/length 2 octets
	vector (protocol ID) 4 octets
	header (source CID) 16 octets
	data variable - may be zero

in subsequent PDUs any field except flags/length may be inherited
*/

#define RLP_FIRSTPDU_MINLENGTH (2 + 4 + sizeof(cid_t))
#define RLP_PDU_MINLENGTH 2

typedef struct
{
	int cookie;
	netSocket_t *sock;
	int dstIP;
	port_t port;
	uint8 packet[MAX_PACKET_SIZE];
} rlp_buf_t;

static uint8 buffers[NUM_PACKET_BUFFERS][MAX_PACKET_SIZE];
static int bufferLengths[NUM_PACKET_BUFFERS];
static int currentBufNum = 0;
static uint8 *packetBuffer;
static int bufferLength;

//static int rlpRxHandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr);

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

void rlpInitSockets(void)
{
	netSocket_t *sockp;

	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
		sockp->localPort = NETI_PORT_NONE;
	rlpInitChannels();
}

//find the socket (if any) with matching local port 
netSocket_t *rlpFindNetSock(port_t localPort)
{
	netSocket_t *sockp;
	
	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
	{
		if(localPort == sockp->localPort)
			return sockp;
	}
	return NULL;
}

netSocket_t *rlpNewNetSock(void)
{
	netSocket_t *sockp;
	
	for (sockp = rlpSockets; sockp < rlpSockets + MAX_RLP_SOCKETS; ++sockp)
	{
		if (sockp->localPort == NETI_PORT_NONE)
			return sockp;
	}
	return NULL;
}

void rlpFreeNetSock(netSocket_t *sockp)
{
	sockp->localPort = NETI_PORT_NONE;
}

/***********************************************************************************************/
/*

The socket/group/channel API is designed to allow dynamic allocation of channels within groups and/or
a linked list or other more sophisticated implementation.
However, this very simplistic (and inefficient for large numbers) implementation
simply maintains an array of channels which are associated with sockets and groups
only by their content

API
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

There is really no independent rlpChannelGroup_t - channelGroup is simply the first
channel in the array with the correct group and socket association
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
void rlpInitChannels(void)
{
	rlpChannel_t *channel;

	for (channel = rlpChannels; channel < rlpChannels + MAX_RLP_CHANNELS; ++channel) channel->socketNum = -1;
}

/*
"Create" a new empty channel group and associate it with a socket and groupAddr
*/
rlpChannelGroup_t *rlpNewChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
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
void rlpFreeChannelGroup(netSocket_t *netsock, rlpChannelGroup_t *channelgroup)
{
	channelgroup->socketNum = -1;
}

/*
find a channelgroup associated with this socket which has the correct groupAddr
*/
rlpChannelGroup_t *rlpFindChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
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
rlpChannel_t *rlpNewChannel(rlpChannelGroup_t *channelgroup)
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
void rlpFreeChannel(rlpChannelGroup_t *channelgroup, rlpChannel_t *channel)
{
	if (channel == channelgroup) channel->protocol = PROTO_NONE;
	else channel->socketNum = -1;
}

/*
Find the first channel in a group with a given protocol
*/
rlpChannel_t *rlpFirstChannel(rlpChannelGroup_t *channelgroup, acnProtocol_t pduProtocol)
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
int sockHasGroups(netSocket_t *netsock)
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
int groupHasChannels(rlpChannelGroup_t *channelgroup)
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

void rlpInitBuffers(void)
{
	int i;
	for (i = 0; i < NUM_PACKET_BUFFERS; i++)
	{
		memcpy(buffers[i], rlpPreamble, PREAMBLE_LENGTH);
		bufferLengths[i] = PREAMBLE_LENGTH;
	}
	currentBufNum = 0;
}

uint8_t *rlpNewPacketBuffer(void)
{
	return buffers[currentBufNum];
}

/***********************************************************************************************/

int initRlp(void)
{	
	static bool initialized = 0;

	if (!initialized)
	{
		rlpInitSockets();
		rlpInitBuffers();

		packetBuffer = rlpNewPacketBuffer();
		initialized = 1;
	}
	return 0;
}

void rlpFlush(void)
{
	bufferLength = PREAMBLE_LENGTH;
}

int rlpEnqueue(int length)
{
	if(bufferLength + length > MAX_PACKET_SIZE)
		return 0;
	else
		bufferLength += length;
	return length;
}

uint8 *rlpFormatPacket(const uint8 *srcCid, int vector)
{
	bufferLength = PREAMBLE_LENGTH + sizeof(uint16); //flags and length
	
	marshalU32(packetBuffer + bufferLength, vector);
	bufferLength += sizeof(uint32);
	
	marshalCID(packetBuffer + bufferLength, (uint8*)srcCid);
	bufferLength += sizeof(uuid_type);
	return packetBuffer + bufferLength;
}

void rlpResendTo(void *sock, uint32 dstIP, uint16 dstPort, int numBack)
{
	int bufToSend;
	
	bufToSend = (NUM_PACKET_BUFFERS - numBack + currentBufNum) & BUFFER_ROLLOVER_MASK;
	sock_sendto((void*)&((netSocket_t *)sock)->nativesock, buffers[bufToSend], bufferLengths[bufToSend], dstIP, dstPort);	// PN-FIXME Waterloo stack call - abstract into netiface
}

int rlpSendTo(void *sock, uint32 dstIP, uint16 dstPort, int keep)
{
	int retVal = currentBufNum;
	
	marshalU16(packetBuffer + PREAMBLE_LENGTH, (bufferLength - PREAMBLE_LENGTH) | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	sock_sendto((void*)&((netSocket_t *)sock)->nativesock, packetBuffer, bufferLength, dstIP, dstPort);    // PN-FIXME Waterloo stack call - abstract into netiface
	
	bufferLengths[currentBufNum] = bufferLength;
	
	if(keep)
	{
		currentBufNum++;
		currentBufNum &= BUFFER_ROLLOVER_MASK;
		packetBuffer = buffers[currentBufNum];
	}
	return retVal;
}

/***********************************************************************************************/
/*
Find a matching netsocket or create a new one if necessary
*/

netSocket_t *rlpOpenNetSocket(port_t localPort)
{
	netSocket_t *netsock;

	if ((netsock = rlpFindNetSock(localPort))) return netsock;	//found existing matching socket

	if ((netsock = rlpNewNetSock()) == NULL) return NULL;			//cannot allocate a new one

	if (neti_udpOpen(netsock, localPort) != 0)
	{
		rlpFreeNetSock(netsock);	//UDP open fails
		return NULL;
	}
	netsock->localPort = localPort;
	return netsock;
}

void rlpCloseNetSocket(netSocket_t *netsock)
{
	neti_udpClose(netsock);
	rlpFreeNetSock(netsock);
}

/***********************************************************************************************/
/*
Find a matching channelgroup or create a new one if necessary
*/

rlpChannelGroup_t *rlpOpenChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
{
	rlpChannelGroup_t *channelgroup;
	if (!isMulticast(groupAddr)) groupAddr = NETI_INADDR_ANY;

	if ((channelgroup = rlpFindChannelGroup(netsock, groupAddr))) return channelgroup;	//found existing matching group

	if ((channelgroup = rlpNewChannelGroup(netsock, groupAddr)) == NULL) return NULL;	//cannot allocate a new one

	if (groupAddr != NETI_INADDR_ANY)
	{
		if (neti_joinGroup(netsock, groupAddr) != 0)
		{
			rlpFreeChannelGroup(netsock, channelgroup);
			return NULL;
		}
	}
	return channelgroup;
}

void rlpCloseChannelGroup(netSocket_t *netsock, rlpChannelGroup_t *channelgroup)
{
	neti_leaveGroup(netsock, channelgroup->groupAddr);
	rlpFreeChannelGroup(netsock, channelgroup);

	if (sockHasGroups(netsock)) return;

	rlpCloseNetSocket(netsock);
}

/***********************************************************************************************/
/*
Create a new channel
*/

rlpChannel_t *rlpOpenChannel(port_t localPort, netAddr_t groupAddr, acnProtocol_t protocol, rlpHandler_t *callback, void *ref)
{
	netSocket_t *netsock;
	rlpChannel_t *channel;
	rlpChannelGroup_t *channelgroup;

	if ((netsock = rlpOpenNetSocket(localPort)) == NULL) goto failsocket;

	if ((channelgroup = rlpOpenChannelGroup(netsock, groupAddr)) == NULL) goto failgroup;

	if ((channel = rlpNewChannel(channelgroup)) == NULL) goto failchannel;

	channel->protocol = protocol;
	channel->callback = callback;
	channel->ref = ref;

	return channel;

failchannel:
	if (!groupHasChannels(channelgroup))
	{
		rlpCloseChannelGroup(netsock, channelgroup);
	}
failgroup:
	if (!sockHasGroups(netsock))
	{
		rlpCloseNetSocket(netsock);
	}
failsocket:
	return NULL;
}

void rlpCloseChannel(rlpChannel_t *channel)
{
	netSocket_t *netsock;
	rlpChannelGroup_t *channelgroup;

	channelgroup = getChannelgroup(channel);
	netsock = getNetsock(channelgroup);

	rlpFreeChannel(channelgroup, channel);
	if (groupHasChannels(channelgroup)) return;

	rlpCloseChannelGroup(netsock, channelgroup);
	if (sockHasGroups(netsock)) return;

	rlpCloseNetSocket(netsock);
}

/***********************************************************************************************/
/*
Process a packet - called by network interface layer on receipt of a packet
*/

void rlpProcessPacket(netSocket_t *netsock, const uint8_t *data, int dataLen, netAddr_t destaddr, const netiHost_t *remhost)
{
	rlpChannelGroup_t *channelgroup;
	rlpChannel_t *channel;
	const cid_t *srcCidp;
	acnProtocol_t pduProtocol;
	const uint8_t *pdup, *datap;
	int datasize;

	pdup = data;
	if(dataLen < RLP_PREAMBLE_LENGTH + RLP_FIRSTPDU_MINLENGTH + RLP_POSTAMBLE_LENGTH)
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: Packet too short to be valid");
		return;	
	}
	//Check and strip off EPI 17 preamble 
	if(memcmp(pdup, rlpPreamble, RLP_PREAMBLE_LENGTH))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: Invalid Preamble");
		return;
	}
	pdup += RLP_PREAMBLE_LENGTH;
	//Find if we have a handler
	
	if ((channelgroup = rlpFindChannelGroup(netsock, destaddr)) == NULL) return;	//No handler for this dest address

	srcCidp = NULL;
	pduProtocol = PROTO_NONE;

	/* first PDU must have all fields */
	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: illegal first PDU flags");
		return;
	}

	/* pdup points to start of PDU */
	while (pdup < data + dataLen)
	{
		uint8_t flags;
		const uint8_t *pp;

		flags = *pdup;
		pp = pdup + 2;
		pdup += getpdulen(pdup);	//pdup now points to end
		if (pdup > data + dataLen)	//sanity check
		{
			syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: packet length error");
			return;
		}
		if (flags & VECTOR_bFLAG)
		{
			pduProtocol = unmarshalU32(pp);
			pp += sizeof(uint32_t);
		}
		if (flags & HEADER_bFLAG)
		{
			srcCidp = (const cid_t *)pp;
			pp += sizeof(uuid_t);
		}
		if (pp > pdup)
		{
			syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: pdu length error");
			return;
		}
		if (flags & DATA_bFLAG)
		{
			datap = pp;
			datasize = pdup - pp;
		}
		/* there may be multiple channels registered for this PDU */
		for (
			channel = rlpFirstChannel(channelgroup, pduProtocol);
			channel != NULL;
			channel = rlpNextChannel(channelgroup, channel, pduProtocol)
		)
		{
			if (channel->callback)
				(*channel->callback)(datap, datasize, channel->ref, remhost, srcCidp);
		}
	}
}

#if 0
/**
 * Handle a received UDP packet
 * @param s the socket
 * @param data the packet buffer
 * @param dataLen the length of the packet
 * @param pseudo the UDP pseudo-header
 * @param hdr the UDP header
 * @return the number of bytes processed
 */
timing_t benchmark;
static char logTimings[200];

static int rlpRxHandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr)
{
	pdu_header_type pduHeader;
	uint32 processed = 0;
	uint8 srcCID[16];
	udp_transport_t transportAddr;

//	memset(&benchmark, 0, sizeof(timing_t));
//	syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: begin");
//	benchmark.packetStart = ticks;
		
	if(dataLen < PREAMBLE_LENGTH + 2)
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Packet too short to contain preamble and pdu");
		return dataLen;	
	}
	
	//Check and strip off EPI 17 preamble 
	if(memcmp(data, rlpPreamble, PREAMBLE_LENGTH))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Invalid Preamble");
		return dataLen;
	}
	
	processed += PREAMBLE_LENGTH;
	
	//determine the ip and port info from the transport protocol
	transportAddr.srcIP = ntohl(((in_Header *)hdr)->source);
	transportAddr.dstIP = ntohl(((in_Header *)hdr)->destination);
	transportAddr.srcPort = ntohs(((udp_Header*)(hdr + in_GetHdrlenBytes((in_Header*)hdr)))->srcPort);
	transportAddr.dstPort = ntohs(((udp_Header*)(hdr + in_GetHdrlenBytes((in_Header*)hdr)))->dstPort);
	
	memset(&pduHeader, 0, sizeof(pdu_header_type));
	while(processed < dataLen)
	{
		//decode the pdu header
		processed += decodePdu(data + processed, &pduHeader, sizeof(uint32), sizeof(uuid_type));
		
//		syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: pdu header decoded");
		if((pduHeader.header == 0) || pduHeader.data == 0) //error
		{
			syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Packet does not contain header and data");
			break;
		}
		memcpy(srcCID, pduHeader.header, sizeof(uuid_type));
		
		//dispatch to vector
		//at the rlp layer vectors are protocolIDs
		switch(pduHeader.vector)
		{
			case PROTO_SDT :
//				syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: Dispatching to SDT");
				sdtRxHandler(&transportAddr, srcCID, pduHeader.data, pduHeader.dataLength);
				break;
			default:
				syslog(LOG_WARNING|LOG_LOCAL0,"rlpRxHandler: Unknown Vector (protocol) - skipping");
		}
	}
	benchmark.packetEnd = ticks;
	if(benchmark.dmpStart)
	{
		sprintf(logTimings, "PS:%d PE:%d SS:%d SE:%d DS:%d DE:%d NP:%d", 
										benchmark.packetStart, 
										benchmark.packetEnd,
										benchmark.sdtStart,
										benchmark.sdtEnd,
										benchmark.dmpStart,
										benchmark.dmpEnd,
										benchmark.numberofdmpprops);
		syslog (LOG_INFO|LOG_LOCAL0, logTimings);
	}
	return dataLen;
}
#endif

