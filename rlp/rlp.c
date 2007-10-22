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

	struct rlpChannelGroup_s represents a multicast group (and port). Any
	netSocket_t may have multiple channel groups. Because of stack vagaries
	with multicast handling, RLP examines and filters the multicast
	destination address of every incoming packet early on and finds the
	associated channelGroup If none is found, the packet is dropped. There is
	one channel group lookup per packet.
	
	struct rlpChannel_s represents a single channel to the client protocol. For each
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
To allow flexibility in implementation, the netSocket_t, struct rlpChannelGroup_s
and struct rlpChannel_s structures used are manipulated using the API of the rlpmem
module - they should not be accessed directly. This allows dynamic or static
allocation strategies, and organization by linked lists, by arrays, or more
complex structures such as trees. In minimal implementations many of these
calls may be overridden by macros for efficiency.

The implementation of these calls is in rlpmem.c where alternative static and
dynamic implementations are provided.

*/
/***********************************************************************************************/

/***********************************************************************************************/
/*
Initialize RLP (if not already done)
*/
int
initRlp(void)
{	
	static bool initialized = 0;

	if (!initialized)
	{
		rlpInitSockets();
		rlpInitBuffers();

		//packetBuffer = rlpNewPacketBuffer();
		initialized = 1;
	}
	return 0;
}

/***********************************************************************************************/
void
rlpFlush(void)
{
	bufferLength = PREAMBLE_LENGTH;
}

/***********************************************************************************************/
int
rlpEnqueue(int length)
{
	if(bufferLength + length > MAX_PACKET_SIZE)
		return 0;
	else
		bufferLength += length;
	return length;
}

/***********************************************************************************************/
uint8_t *
rlpFormatPacket(const uint8_t *srcCid, int vector)
{
	bufferLength = PREAMBLE_LENGTH + sizeof(uint16_t); /* flags and length */
	
	marshalU32(packetBuffer + bufferLength, vector);
	bufferLength += sizeof(uint32_t);
	
	marshalCID(packetBuffer + bufferLength, (uint8_t*)srcCid);
	bufferLength += sizeof(uuid_t);
	return packetBuffer + bufferLength;
}

/***********************************************************************************************/
void
rlpResendTo(void *sock, uint32_t dstIP, uint16_t dstPort, int numBack)
{
	int bufToSend;
	
	bufToSend = (NUM_PACKET_BUFFERS - numBack + currentBufNum) & BUFFER_ROLLOVER_MASK;
	sock_sendto((void*)&((netSocket_t *)sock)->nativesock, buffers[bufToSend], bufferLengths[bufToSend], dstIP, dstPort);	/* PN-FIXME Waterloo stack call - abstract into netiface */
}

/***********************************************************************************************/
int 
rlpSendTo(void *sock, uint32_t dstIP, uint16_t dstPort, int keep)
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
If multiple Root Layer PDUs are to be packed into the buffer, the client may call
rlpInitBlock(startptr) then each time it has completed a PDU it calls rlpAddPDU(size,
pduPtrPtr). The latter call returns a pointer to the location to place the next PDU. If
pduPtrPtr is not null, the pointer it points to is filled in with the final position of the data
portion of this PDU allowing it to be subsequently used again.

for example.

	mybuf = rlpNewTxBuf(200);
	pduptr = rlpInitBlock(mybuf);
	memcpy(pduptr, somePDUcontent, PDU1size);
	pduptr = rlpAddPDU(mybuf, pduptr, PDU1size, NULL);
	memcpy(pduptr, morePDUcontent, PDU2size);
	pduptr = rlpAddPDU(mybuf, pduptr, PDU2size, &savedDataPtr);
	rlpTxPDUblock(mybuf);

struct rlpTxbufhdr_s {
	struct rlpTxbuf_s *next;
	usage_t usage;
	uint16_t datasize;
	uint8_t *blockstart;
	uint8_t *blockend;
	acnProtocol_t protocol;
	uint8_t *vector;
	uint8_t *header;
	uint8_t *data;
	int	datasize;
};
*/
/*
Start an RLP PDU block
*/
uint8_t *
rlpInitBlock(struct rlpTxbuf *buf)
{
	
	if (data >= rlpItsData(buf) + ((struct rlpTxbufhdr_s *)(buf))->datasize)
		return -1;
	bufhdrp(buf)->datastart = data;
	return 0;
}

/***********************************************************************************************/
/*
*/

uint8_t *
rlpAddPDU(struct rlpTxbuf *buf, acnProtocol_t protocol, uint8_t *pdudata, int size)
{
	int dataoffset;
	uint16_t flags, length;
	uint8_t *pdup;

	length = 2;	/* allow for length field */
	flags = 0;
	pdup = bufhdrp(buf)->blockend;

	/* do we have a vector? */
	if (protocol != bufhdrp(buf)->protocol)
	{
		bufhdrp(buf)->protocol = protocol;
		flags |= VECTOR_FLAG;
		length += sizeof(acnProtocol_t);
	}
	/* do we need a header? Only in first of block since CIDs cannot share buffers */
	if (pdup == bufhdrp(buf)->blockstart)

	/* does data match previous? Assume not */

	/* is data in the right place already? */
	if (pdudata != pdup + length)
	{
		/* move data into position */
	}
	length += size;
	marshalU16(pdup, (uint16_t)(length | flags));
	pdup += 2;
	if ((flags & VECTOR_FLAG))
	{
		marshallU32(pdup, protocol);
		pdup += sizeof(acnProtocol_t);
	}
	
		
}

/***********************************************************************************************/
/*
Find a matching netsocket or create a new one if necessary
*/

netSocket_t *
rlpOpenNetSocket(port_t localPort)
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

/***********************************************************************************************/
void 
rlpCloseNetSocket(netSocket_t *netsock)
{
	neti_udpClose(netsock);
	rlpFreeNetSock(netsock);
}

/***********************************************************************************************/
/*
Find a matching channelgroup or create a new one if necessary
*/

struct rlpChannelGroup_s *
rlpOpenChannelGroup(netSocket_t *netsock, netAddr_t groupAddr)
{
	struct rlpChannelGroup_s *channelgroup;
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

/***********************************************************************************************/
void 
rlpCloseChannelGroup(netSocket_t *netsock, struct rlpChannelGroup_s *channelgroup)
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

struct rlpChannel_s *
rlpOpenChannel(port_t localPort, netAddr_t groupAddr, acnProtocol_t protocol, rlpHandler_t *callback, void *ref)
{
	netSocket_t *netsock;
	struct rlpChannel_s *channel;
	struct rlpChannelGroup_s *channelgroup;

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

/***********************************************************************************************/
void 
rlpCloseChannel(struct rlpChannel_s *channel)
{
	netSocket_t *netsock;
	struct rlpChannelGroup_s *channelgroup;

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

void
rlpProcessPacket(netSocket_t *netsock, const uint8_t *data, int dataLen, netAddr_t destaddr, const netiHost_t *remhost)
{
	struct rlpChannelGroup_s *channelgroup;
	struct rlpChannel_s *channel;
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

