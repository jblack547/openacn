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

/* syslog facility LOG_LOCAL0 is used for ACN:RLP */

#define MAX_PACKET_SIZE 1450
#define PREAMBLE_LENGTH 16

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

#define NUM_PACKET_BUFFERS	16
#define BUFFER_ROLLOVER_MASK  (NUM_PACKET_BUFFERS - 1)

#ifdef CONFIG_EPI17

static const uint8_t rlpPreamble[RLP_PREAMBLE_LENGTH] =
{
	0, RLP_PREAMBLE_LENGTH,		/* Network byte order */
	0, RLP_POSTAMBLE_LENGTH,	/* Network byte order */
	'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', '\0', '\0', '\0'
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

static uint8_t buffers[NUM_PACKET_BUFFERS][MAX_PACKET_SIZE];
static int bufferLengths[NUM_PACKET_BUFFERS];
static int currentBufNum = 0;
static uint8_t *packetBuffer;
static int bufferLength;

/*

API description

Any higher protocol registers with RLP by calling rlpOpenChannel(). This
establishes a channel consisting of a local port, a local (incoming)
multicast group (or NETI_INADDR_ANY for unicast) and a protocol. The client
protocol must open a separate channel for each port/group/protocol
combination it uses.

Incoming packets
--
When opening a channel the client protocol provides a callback function and
reference pointer. On receipt of a PDU matching that channel parameters, RLP
calls the callback function passing the reference pointer as a parameter.
This  pointer can be used for any purpose by the higher layer - it is treated
as opaque  data by RLP. The callback is also passed pointers to a structure
(network layer dependent) representing the transport address of the sender,
and to the CID of the remote component.

In order to manage the relationship between channels at the client protocol
side the network interface layer, RLP maintains three
structures:

	netSocket_t represents an interface to the netiface layer - there is one
	netSocket_t for each incoming (local) port.

	rlpChannelGroup_t represents a multicast group (and port). Any
	netSocket_t may have multiple channel groups. Because of stack vagaries
	with multicast handling, RLP examines and filters the multicast
	destination address of every incoming packet early on and finds the
	associated channelGroup If none is found, the packet is dropped. There is
	one channel group lookup per packet.
	
	rlpChannel_t represents a single channel to the client protocol. For each
	channel group there may be multiple channels. Channels are filtered for
	protocol (e.g. SDT) on a PDU by PDU basis. It is permitted to open
	multiple channels for the same port/group/protocol - the same PDU
	contents are passed to each in turn.

All parameters passed to the client protocol are passed by reference (e.g. a
pointer to the sender CID is passed, not the CID itself).  They must be
treated as constant data and not modified by the client. However, they do not
persist across calls so the client must copy any items which it requires to
keep.

*/

/***********************************************************************************************/
/*

Management of structures in memory
--
To allow flexibility in implementation, the netSocket_t, rlpChannelGroup_t
and rlpChannel_t structures used are manipulated using the API of the rlpmem
module - they should not be accessed directly. This allows dynamic or static
allocation strategies, and organization by linked lists, by arrays, or more
complex structures such as trees. In minimal implementations many of these
calls may be overridden by macros for efficiency.

The implementation of these calls is in rlpmem.c where alternative static and
dynamic implementations are provided.

*/
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

uint8_t *rlpFormatPacket(const uint8_t *srcCid, int vector)
{
	bufferLength = PREAMBLE_LENGTH + sizeof(uint16_t); /* flags and length */
	
	marshalU32(packetBuffer + bufferLength, vector);
	bufferLength += sizeof(uint32_t);
	
	marshalCID(packetBuffer + bufferLength, (uint8_t*)srcCid);
	bufferLength += sizeof(uuid_t);
	return packetBuffer + bufferLength;
}

void rlpResendTo(void *sock, uint32_t dstIP, uint16_t dstPort, int numBack)
{
	int bufToSend;
	
	bufToSend = (NUM_PACKET_BUFFERS - numBack + currentBufNum) & BUFFER_ROLLOVER_MASK;
	sock_sendto((void*)&((netSocket_t *)sock)->nativesock, buffers[bufToSend], bufferLengths[bufToSend], dstIP, dstPort);	/* PN-FIXME Waterloo stack call - abstract into netiface */
}

int rlpSendTo(void *sock, uint32_t dstIP, uint16_t dstPort, int keep)
{
	int retVal = currentBufNum;
	
	marshalU16(packetBuffer + PREAMBLE_LENGTH, (bufferLength - PREAMBLE_LENGTH) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
	sock_sendto((void*)&((netSocket_t *)sock)->nativesock, packetBuffer, bufferLength, dstIP, dstPort);    /* PN-FIXME Waterloo stack call - abstract into netiface */
	
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

	if ((netsock = rlpFindNetSock(localPort))) return netsock;	/* found existing matching socket */

	if ((netsock = rlpNewNetSock()) == NULL) return NULL;			/* cannot allocate a new one */

	if (neti_udpOpen(netsock, localPort) != 0)
	{
		rlpFreeNetSock(netsock);	/* UDP open fails */
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

	if ((channelgroup = rlpFindChannelGroup(netsock, groupAddr))) return channelgroup;	/* found existing matching group */

	if ((channelgroup = rlpNewChannelGroup(netsock, groupAddr)) == NULL) return NULL;	/* cannot allocate a new one */

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
	/* Check and strip off EPI 17 preamble  */
	if(memcmp(pdup, rlpPreamble, RLP_PREAMBLE_LENGTH))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpProcessPacket: Invalid Preamble");
		return;
	}
	pdup += RLP_PREAMBLE_LENGTH;
	/* Find if we have a handler */
	
	if ((channelgroup = rlpFindChannelGroup(netsock, destaddr)) == NULL) return;	/* No handler for this dest address */

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
		pdup += getpdulen(pdup);	/* pdup now points to end */
		if (pdup > data + dataLen)	/* sanity check */
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

static int rlpRxHandler(void *s, uint8_t *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr)
{
	pdu_header_type pduHeader;
	uint32_t processed = 0;
	uint8_t srcCID[16];
	udp_transport_t transportAddr;

/* 	memset(&benchmark, 0, sizeof(timing_t)); */
/* 	syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: begin"); */
/* 	benchmark.packetStart = ticks; */
		
	if(dataLen < PREAMBLE_LENGTH + 2)
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Packet too short to contain preamble and pdu");
		return dataLen;	
	}
	
	/* Check and strip off EPI 17 preamble  */
	if(memcmp(data, rlpPreamble, PREAMBLE_LENGTH))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Invalid Preamble");
		return dataLen;
	}
	
	processed += PREAMBLE_LENGTH;
	
	/* determine the ip and port info from the transport protocol */
	transportAddr.srcIP = ntohl(((in_Header *)hdr)->source);
	transportAddr.dstIP = ntohl(((in_Header *)hdr)->destination);
	transportAddr.srcPort = ntohs(((udp_Header*)(hdr + in_GetHdrlenBytes((in_Header*)hdr)))->srcPort);
	transportAddr.dstPort = ntohs(((udp_Header*)(hdr + in_GetHdrlenBytes((in_Header*)hdr)))->dstPort);
	
	memset(&pduHeader, 0, sizeof(pdu_header_type));
	while(processed < dataLen)
	{
		/* decode the pdu header */
		processed += decodePdu(data + processed, &pduHeader, sizeof(uint32_t), sizeof(uuid_type));
		
/* 		syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: pdu header decoded"); */
		if((pduHeader.header == 0) || pduHeader.data == 0) /* error */
		{
			syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Packet does not contain header and data");
			break;
		}
		memcpy(srcCID, pduHeader.header, sizeof(uuid_type));
		
		/* dispatch to vector */
		/* at the rlp layer vectors are protocolIDs */
		switch(pduHeader.vector)
		{
			case PROTO_SDT :
/* 				syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: Dispatching to SDT"); */
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

