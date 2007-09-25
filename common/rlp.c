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

#define MAX_RLP_SOCKETS 50
#define MAX_PACKET_SIZE 1450
#define PREAMBLE_LENGTH 16

//#define __WATTCP_KERNEL__ // for access to the tcpHeader
//#include <wattcp.h>
#include <string.h>
#include "rlp.h"
#include "pdu.h"
#include "sdt.h"
#include "marshal.h"
#include <syslog.h>

typedef struct 
{
	udp_Socket s;
	
	uint16 useCount;
	port_t localPort;
} rlp_socket_t;

#define BUFFER_ROLLOVER_MASK  (NUM_PACKET_BUFFERS - 1)

static const char epi17preamble[PREAMBLE_LENGTH]=
{
	0x00, 0x10,	//Size of Preamble - 16 Bytes
	0x00, 0x00, //Size of Postamble - 0 Bytes
	0x41,			//'A'
	0x53,			//'S'
	0x43,			//'C'
	0x2D,			//'-'
	0x45,			//'E'
	0x31,			//'1'
	0x2E,			//'.'
	0x31,			//'1'
	0x37,			//'7'
	0x00,			//
	0x00,			//
	0x00,			//
};

typedef struct
{
	int cookie;
	rlp_socket_t *sock;
	int dstIP;
	port_t port;
	uint8 packet[MAX_PACKET_SIZE];
}rlp_buf_t;

static uint8 buffers[NUM_PACKET_BUFFERS][MAX_PACKET_SIZE];
static int bufferLengths[NUM_PACKET_BUFFERS];
static int currentBufNum = 0;
static uint8 *packetBuffer;
static int bufferLength;

static int rlpRxHandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr);

/***********************************************************************************************/
/*
Socket memory currently static array, but accessed only via
rlpInitSockets, rlpFindSocket, rlpNewSocket, rlpFreeSocket
so could change to list or other implementation

This currently also suffers from the Shlemeil the Painter problem
*/

static rlp_socket_t rlpSockets[MAX_RLP_SOCKETS];

void rlpInitSockets(void)
{
	rlp_socket_t sockp;

	for (sockp rlpSockets; sockp < (rlpSockets + MAX_RLP_SOCKETS); ++sockp)
		sockp->useCount = 0;
}

//find the socket (if any) with matching local port 
rlp_socket_t *rlpFindSocket(port_t localPort)
{
	rlp_socket_t sockp;
	
	for (sockp rlpSockets; sockp < (rlpSockets + MAX_RLP_SOCKETS); ++sockp)
	{
		if(sockp->useCount && localPort == sockp->localPort)
			return sockp;
	}
	return NULL;
}

rlp_socket_t *rlpNewSocket(void)
{
	rlp_socket_t sockp;
	
	for (sockp rlpSockets; sockp < (rlpSockets + MAX_RLP_SOCKETS); ++sockp)
	{
		if (sockp->useCount == 0)
			return sockp;
	}
	return NULL;
}

void rlpFreeSocket(rlp_socket_t *sockp)
{
}

/***********************************************************************************************/

void initRlp(void)
{
	int i;
	
	rlpInitSockets();

	for (i = 0; i < NUM_PACKET_BUFFERS; i++)
	{
		memcpy(buffers[i], epi17preamble, PREAMBLE_LENGTH);
		bufferLengths[i] = PREAMBLE_LENGTH;
	}
	rlpOpenSocket(LOCAL_ADHOC_PORT);	// PN-FIXME LOCAL_ADHOC_PORT is part of SDT - only open a socket when SDT registers with RLP
	currentBufNum = 0;
	packetBuffer = buffers[currentBufNum];

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
	sock_sendto((void*)&((rlp_socket_t *)sock)->s, buffers[bufToSend], bufferLengths[bufToSend], dstIP, dstPort);	// PN-FIXME Waterloo stack call - abstract into netiface
}

int rlpSendTo(void *sock, uint32 dstIP, uint16 dstPort, int keep)
{
	int retVal = currentBufNum;
	
	marshalU16(packetBuffer + PREAMBLE_LENGTH, (bufferLength - PREAMBLE_LENGTH) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
	sock_sendto((void*)&((rlp_socket_t *)sock)->s, packetBuffer, bufferLength, dstIP, dstPort);    // PN-FIXME Waterloo stack call - abstract into netiface
	
	bufferLengths[currentBufNum] = bufferLength;
	
	if(keep)
	{
		currentBufNum++;
		currentBufNum &= BUFFER_ROLLOVER_MASK;
		packetBuffer = buffers[currentBufNum];
	}
	return retVal;
}

void *rlpOpenSocket(uint16 localPort)
{
	rlp_socket_t *socket;
	int i;
	
	if (!localPort)
		localPort = ACN_RESERVED_PORT;

	//check if this socket already exists
	socket = rlpFindSocket(localPort);
	if(!socket) //no socket found
	{
		socket = rlpNewSocket();
		if (!socket) return NULL;
		udp_open(&socket->s, localPort, -1, 0, rlpRxHandler);	// PN-FIXME Waterloo stack call - abstract into netiface
		socket->localPort = localPort;
	}
	(socket->useCount)++;
	return socket;
}

void rlpCloseSocket(void *s)
{
	rlp_socket_t *socket = s;
	(socket->useCount)--;
	if(!socket->useCount) //no further users of this socket
	{
		//close the socket
		sock_close((void*)&(socket->s));    // PN-FIXME Waterloo stack call - abstract into netiface
		rlpFreeSocket(socket);
	}
}

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

	memset(&benchmark, 0, sizeof(timing_t));
//	syslog(LOG_DEBUG|LOG_LOCAL0,"rlpRxHandler: begin");
	benchmark.packetStart = ticks;
		
	if(dataLen < PREAMBLE_LENGTH + 2)
	{
		syslog(LOG_ERR|LOG_LOCAL0,"rlpRxHandler: Packet too short to contain preamble and pdu");
		return dataLen;	
	}
	
	//Check and strip off EPI 17 preamble 
	if(memcmp(data, epi17preamble, PREAMBLE_LENGTH))
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
