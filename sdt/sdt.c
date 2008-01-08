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
//;//syslog facility LOG_LOCAL1 is used for ACN:SDT

#include "sdt.h"
#include "acn_sdt.h"
#include <string.h>
#include <swap.h>
#include "dmp.h"
#include <acn_config.h>
#include <crond.h>
#include <syslog.h>
#include <ppmalloc.h>
#include <stdio.h>
#include <slp.h>
#include <rtclock.h>
#include <marshal.h>
#include <syslog.h>
#include "mcast_util.h"


typedef struct foreign_member_t
{
        struct foreign_member_t *next;
        foreign_component_t *component;
        uint16 mid;
        
        uint8 nak:1;
        uint8 pending:1;
        uint8 isLocal:1;
        uint8        isConnected:1;
        uint8 expiryTime;
        
        uint32 expiresAt;        

} foreign_member_t;

typedef struct local_member_t
{
        struct local_member_t *next;
        local_component_t *component;
        uint16 mid;
        
        uint8 nak:1;
        uint8 pending:1;
        uint8 isLocal:1;
        uint8 isConnected:1;
        
        uint8 expiryTime;
        
        uint32 expiresAt;

        uint16 nakHoldoff;
        uint16 lastAcked;
        
        uint16 nakModulus;
        uint16 nakMaxWait;
        
} local_member_t;

typedef struct channel_t
{
        struct channel_t *next;

        uint16 channelNumber;
        uint16 availableMid;
        
        uint8  pending;
        uint8  state;
        uint16 downstreamPort;
        
        uint32 downstreamIP;
        
        uint16 isLocal;
        
        uint32 totalSeq;
        uint32 reliableSeq;
        uint32 oldestAvail;
        local_member_t *memberList;
        void         *sock;
} channel_t;

typedef struct leader_t
{
        struct leader_t *next;
        void *component;
        channel_t *channelList;
} leader_t;

typedef struct session_t
{
        struct session_t *next;
        leader_t *localLeader;
        leader_t *foreignLeader;
        channel_t *localChannel;
        channel_t *foreignChannel;
        uint32 protocol;
} session_t;

uint16 initSdt(void);
void shutdownSdt(void);

int sdtRxHandler(udp_transport_t *transportAddr, uint8 srcCID[16], uint8 *data, uint16 dataLen);
//uint16 sdtJoinRequest(pdu_header_type *header);
uint32 sdtTxUpstream(local_component_t *srcComp, foreign_component_t *dstComp, channel_t *channel, uint8 *data, uint32 dataLength);
void txQueuedBlocks(uint32 queuedProto, uint16 relatedChannelNumber, uint16 firstMid, uint16 lastMid);
uint8 *sdtFormatWrapper(int isReliable, local_component_t *srcComp, uint16 firstMid, uint16 lastMid, uint16 makThreshold);
uint8 *sdtFormatClientBlock(foreign_component_t *dstComp, uint32 protocol, channel_t *foreignChannel);
uint8 *enqueueClientBlock(int dataLength);
void sdtFlush(void);

uint8 *getPduBuffer(void);
uint8 *enqueuePdu(uint16 pduLength);
uint16 remainingPacketBuffer();
void delSdtMember(uint8 cid[16]);
//component_t *registerSdtDevice(uint8 cid[16]);
//component_t *unregisterSdtDevice(uint8 cid[16]);

static leader_t leaderMemory[MAX_LEADERS];
static channel_t channelMemory[MAX_CHANNELS];
static local_member_t localMemberMemory[MAX_LOCAL_MEMBERS];
static foreign_member_t foreignMemberMemory[MAX_FOREIGN_MEMBERS];

static leader_t *leaders;

//Support Functions
static inline leader_t *findLeader(void *component);
static inline leader_t *addLeader(void *component);
static inline void removeLeader(leader_t *leader);

static inline channel_t *findChannel(leader_t *leader, uint16 channelNumber);
static inline channel_t *addChannel(leader_t *leader, uint16 channelNumber);
static inline void removeChannel(leader_t *leader,  channel_t *channel);

static inline void *findMemberByMid(channel_t *channel, uint16 mid);
static inline void *findMemberByComponent(channel_t *channel, void *component);
static inline local_member_t *addLocalMember(channel_t *channel);
static inline foreign_member_t *addForeignMember(channel_t *channel);
static inline void removeLocalMember(channel_t *channel, local_member_t *member);
static inline void removeForeignMember(channel_t *channel, foreign_member_t *member);

static inline int marshalChannelParamBlock(uint8 *paramBlock);
static inline int unmarshalChannelParamBlock(local_member_t *member, uint8 *paramBlock);

static inline int marshalTransportAddress(uint8 *data, uint16 port, uint32 ipAddress, int type);
static inline int unmarshalTransportAddress(uint8 *data, udp_transport_t *transportAddr, uint16 *port, uint32 *ipAddress, int *type);

static void sdtTick(void);
static void sdtTxJoin(local_component_t *lComp, channel_t *lChannel, foreign_component_t *fComp, channel_t *fChannel);
static uint32 sdtRxJoin(udp_transport_t *transportAddr, uint8 srcCID[16], pdu_header_type *header);
static void sdtTxJoinAccept(foreign_component_t *fComp, channel_t *fChannel, local_member_t *lMember, channel_t *lChannel);
static uint32 sdtRxJoinAccept(uint8 *srcCID, pdu_header_type *header);
static void sdtTxJoinRefuse(udp_transport_t *transportAddr, uint8 srcCID[16], pdu_header_type *header, uint8 reason);
static uint32 sdtRxJoinRefuse(uint8 *srcCID, pdu_header_type *header);

static int sdtRxWrapper(udp_transport_t *transportAddr, uint8 *srcCID, pdu_header_type *wrapper);
static void sdtClientRxHandler(leader_t *leader, channel_t *channel, local_member_t *member, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength);
static inline uint32 checkSequence(channel_t *s, uint8 isReliable, uint32 rxTotal, uint32 rxReliable, uint32 oldestAvail);
static void sdtTxNak(leader_t *leader, channel_t *channel, local_member_t *member, uint32 lastMissed);
static void sdtRxNak(uint8 *srcCID, pdu_header_type *header);

static uint8 *sdtTxAck(foreign_component_t *dstComp, channel_t *foreignChannel);
static void sdtRxAck(foreign_member_t *remoteMember, uint8 *data, int dataLength);

static void sdtRxConnect(leader_t *remoteLeader, channel_t *remoteChannel, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength);
static void sdtTxConnect(leader_t *localLeader, channel_t *localChannel, foreign_component_t *dstComp, channel_t *foreignChannel, uint32 protocol);
static void sdtRxConnectAccept(foreign_member_t *member, uint8 *data, int dataLength);
//static void sdtTxLeave(leader_t *leader, channel_t *channel, foreign_member_t *member);
static void sdtRxLeave(leader_t *remoteLeader, channel_t *remoteChannel, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength);
static void sdtTxLeaving(leader_t *leader, channel_t *channel, local_member_t *member, uint8 reason);
static int sdtRxLeaving(uint8 *srcCID, pdu_header_type *header);

enum
{
	SDT_STATE_NON_MEMBER,
	SDT_STATE_JOIN_SENT,
	SDT_STATE_JOIN_ACCEPT_SENT,
	SDT_STATE_JOIN_RECEIVED,
	SDT_STATE_CHANNEL_OPEN,
	SDT_STATE_LEAVING,	
};



/* SDT message fields */
/* Join */
#define mJOIN_CID			0
#define mJOIN_MID			16
#define mJOIN_CHANNEL		18
#define mJOIN_RECIPROCAL	20
#define mJOIN_TOTALSEQ		22
#define mJOIN_RELSEQ		26
#define mJOIN_DESTADDR		30
#define mJOIN_CHANNELPARAM	30 + 
#define mJOIN_
#define mJOIN_
#define mJOIN_
#define mJOIN_

typedef int sdtMsgDespatcher(
	const uint8_t *data,
	int datasize,
	void *ref,
	const netiHost_t *remhost,
	const cid_t *remcid
);

struct msgDespatch_s {
	uint8_t msgCode;
	sdtMsgDespatcher *func;
	void *funcref;
};

const struct msgDespatch_s adhocMsgTab[] = {
	{SDT_JOIN, &sdtRxJoin, NULL},
//	{SDT_GET_SESSIONS, },	currently unsupported
//	{SDT_SESSIONS, },	only if we ever send SDT_GET_SESSIONS
	{0, NULL, NULL}
};

void
adhocRxHandler(
	const uint8_t *data,
	int datasize,
	void *ref,
	const netiHost_t *remhost,
	const cid_t *remcid
)
{
	const uint8_t *pdup, *pdudata;
	int pdudatalen;
	uint8_t flags;
	const uint8_t *pp;
	uint8_t msgtype;
	struct msgDespatch_s *pdufunc;

	pdup = data;
	if (datasize < SDT_FIRSTPDU_MINLENGTH)
	{
		syslog(LOG_ERR|LOG_LOCAL0,"adhocRxHandler: PDU too short to be valid");
		return;	
	}

	/* first PDU must have all fields */
	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG))
	{
		syslog(LOG_ERR|LOG_LOCAL0,"adhocRxHandler: illegal first PDU flags");
		return;
	}

	sdtMsg = NULL;
	/* pdup points to start of PDU */
	while (pdup < data + datasize)
	{
		flags = *pdup;
		pp = pdup + 2;
		pdup += getpdulen(pdup);	//pdup now points to end
		if (pdup > data + datasize)	//sanity check
		{
			syslog(LOG_ERR|LOG_LOCAL0,"adhocRxHandler: packet length error");
			return;
		}
		if (flags & VECTOR_bFLAG)
		{
			pdufunc = findMsgfunc(*pp++, (const struct msgDespatch_s *)ref);	//vector is 1 byte only for SDT base layer
		}
		/* No header for SDT base layer */
		if (flags & DATA_bFLAG)
		{
			pdudata = pp;
			pdudatalen = pdup - pp;
		}
		if (pdufunc && pdufunc->func)
			(*pdufunc->func)(pdudata, pdudatalen, pdufunc->funcref, remhost, srcCidp);
	}
	
}


int sdtRxHandler(udp_transport_t *transportAddr, uint8 srcCID[16], uint8 *data, uint16 dataLen)
{
	uint32 processed = 0;
	pdu_header_type pduHeader;
	
	benchmark.sdtStart = ticks;
	
	memset(&pduHeader, 0, sizeof(pdu_header_type));
	
	while(processed < dataLen)
	{
		//decode the pdu header
		processed += decodePdu(data + processed, &pduHeader, sizeof(uint8), 0); //8 bit vectors, no headers
		

		//dispatch to vector

		//at the sdt root layer vectors are commands or wrappers
		switch(pduHeader.vector)
		{
			case SDT_JOIN :
				//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtRxJoin");
				sdtRxJoin(transportAddr, srcCID, &pduHeader);
				break;
			case SDT_JOIN_ACCEPT :
				//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtJoinAccept");
				sdtRxJoinAccept(srcCID, &pduHeader);
				break;
			case SDT_JOIN_REFUSE :
				//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtJoinRefuse");
				sdtRxJoinRefuse(srcCID, &pduHeader);
				break;
			case SDT_LEAVING :
				//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtRxLeaving");
				sdtRxLeaving(srcCID, &pduHeader);
				break;
			case SDT_NAK :
				;//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtRxNack");
				sdtRxNak(srcCID, &pduHeader);
				break;
			case SDT_REL_WRAPPER :
			case SDT_UNREL_WRAPPER :
				;//syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Dispatch to sdtRxWrapper");
				sdtRxWrapper(transportAddr, srcCID, &pduHeader);
				break;
			case SDT_GET_SESSIONS :
				syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Get Sessions unsupported");
				break;
			case SDT_SESSIONS :
				syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: Sessions??? I don't want any of that !");
				break;
			default:
				syslog(LOG_WARNING|LOG_LOCAL1,"sdtRxHandler: Unknown Vector (protocol) - skipping");
		}

	}
	if (processed != dataLen)
	{
		syslog(LOG_ERR|LOG_LOCAL1,"sdtRxHandler: processed an incorrect number of bytes");
	}
	benchmark.sdtEnd = ticks;
//	syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxHandler: End");
	return dataLen;
}

enum
{
	SEQ_VALID,
	SEQ_NAK,
	SEQ_DUPLICATE,
	SEQ_OLD_PACKET,
	SEQ_LOST,
};


static leader_t *currentLeader = 0;
static local_component_t *currentSrc = 0;
static channel_t *currentChannel =0;
static uint8 *currentBuffer = 0;
static uint8 *currentWrapper = 0;
static uint8* currentClientBlock = 0;
static int currentIsReliable = 0;

uint8 *sdtFormatWrapper(int isReliable, local_component_t *srcComp, uint16 firstMid, uint16 lastMid, uint16 makThreshold)
{
//	foreign_member_t *member;
//	uint16 dstMid;
	//find the local leader and a channel to the remote component
	if (srcComp != currentSrc)
	{
		//FIXME - force a tx of the previous queued blocks
		currentSrc = srcComp;
		currentLeader = findLeader(srcComp);
		if (!currentLeader)
		{
				syslog(LOG_LOCAL1 | LOG_ERR, "sdtFormatWrapper : failed to find local leader");
				return 0;
		}
		currentChannel = currentLeader->channelList;
		if (!currentChannel)
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtFormatWrapper : failed to find local channel");
			return 0;
		}
	}
	currentBuffer = rlpFormatPacket(srcComp->cid, PROTO_SDT);
	currentWrapper = currentBuffer;
	
	currentBuffer += sizeof(uint16);
	currentIsReliable = isReliable;
	
	if (currentIsReliable)
	{
		currentChannel->reliableSeq++;
		currentChannel->oldestAvail = (currentChannel->reliableSeq - currentChannel->oldestAvail == NUM_PACKET_BUFFERS) ? 
												currentChannel->oldestAvail + 1 : currentChannel->oldestAvail;
	}
	currentChannel->totalSeq++;

	
	currentBuffer += marshalU8(currentBuffer, (isReliable) ? SDT_REL_WRAPPER : SDT_UNREL_WRAPPER);

	currentBuffer += marshalU16(currentBuffer, currentChannel->channelNumber);
	currentBuffer += marshalU32(currentBuffer, currentChannel->totalSeq);
	currentBuffer += marshalU32(currentBuffer, currentChannel->reliableSeq);
	currentBuffer += marshalU32(currentBuffer, currentChannel->oldestAvail); //FIXME - Change to oldest available when buffering support added
	currentBuffer += marshalU16(currentBuffer, firstMid);
	currentBuffer += marshalU16(currentBuffer, lastMid);
	currentBuffer += marshalU16(currentBuffer, makThreshold);
	currentClientBlock = (firstMid == 0xFFFF) ? 0 : (void*)1; //force this wrapper to transmit if a mak is requested (even if nothing else is added)
	return currentBuffer;
}

static foreign_component_t *currentDstComp = 0;
static uint32 currentProtocol = 0;
static channel_t *currentforeignChannel = 0;

uint8 *sdtFormatClientBlock(foreign_component_t *dstComp, uint32 protocol, channel_t *foreignChannel)
{
	foreign_member_t *member;
	
	if ((currentDstComp == dstComp) && (currentProtocol == protocol) && (currentforeignChannel == foreignChannel))
		return currentBuffer;

	currentClientBlock = currentBuffer;
	currentDstComp = dstComp;
	currentProtocol = protocol;
	currentforeignChannel = foreignChannel;

	currentBuffer += 2; //flags and length
	
	if (dstComp) //allows for a broadcast to all members of the leaders channel
	{
		member = findMemberByComponent(currentChannel, dstComp);
		if (!member) //we obviously don't have a channel to this comp
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtFormatClientBlock : failed to find specified member -- this will cause death");
			return 0;
		}
		currentBuffer += marshalU16(currentBuffer, member->mid); //Vector
	}
	else
	{
		currentBuffer += marshalU16(currentBuffer, 0xFFFF); //Vector
	}
	
	currentBuffer += marshalU32(currentBuffer, protocol); //Header
	currentBuffer += marshalU16(currentBuffer, (foreignChannel) ? foreignChannel->channelNumber : 0); //Header
	return currentBuffer;
}

uint8 *enqueueClientBlock(int dataLength)
{
	uint16 clientBlockLength;
	currentBuffer += dataLength;
	clientBlockLength = currentBuffer - currentClientBlock;
	marshalU16(currentClientBlock, clientBlockLength | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	return currentBuffer;
}


void sdtTxCurrentWrapper(void)
{
	uint16 wrapperLength;

	
	if (!currentClientBlock) //empty wrapper don't tx
	{
		if (currentIsReliable)
			currentChannel->reliableSeq--;
		currentChannel->totalSeq--;
		return;
	}
	wrapperLength = currentBuffer - currentWrapper;
	marshalU16(currentWrapper, wrapperLength /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	

	rlpEnqueue(wrapperLength);
	rlpSendTo(currentChannel->sock, currentChannel->downstreamIP, currentChannel->downstreamPort, currentIsReliable);
	currentSrc = 0;
	currentDstComp = 0;
	currentProtocol = 0;
	currentforeignChannel = 0;
	currentClientBlock = 0;
}

void sdtFlush(void)
{
	sdtTxCurrentWrapper();
}

static uint8 *sdtTxAck(foreign_component_t *dstComp, channel_t *foreignChannel)
{
	uint8 *buf;
	
	buf = sdtFormatClientBlock(dstComp, PROTO_SDT, foreignChannel);
	buf += marshalU16(buf, 7 | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buf += marshalU8(buf, SDT_ACK);
	buf += marshalU32(buf, foreignChannel->reliableSeq);
	
	return enqueueClientBlock(7);
}
//static int packetCount = 0;
static int sdtRxWrapper(udp_transport_t *transportAddr, uint8 *srcCID, pdu_header_type *wrapper)
{
	int processed = 0;
	
	pdu_header_type clientPdu;
	
	foreign_component_t *srcComp;
	leader_t *remoteLeader;
	channel_t *remoteChannel;
	leader_t *localLeader;
	channel_t *localChannel;
	uint16 channelNumber;
	uint32 totalSeq;
	uint32 reliableSeq;
	uint32 oldestAvail;
	uint32 clientProtocol;
	uint16 relatedChannel;
	uint16 firstMid = 0;
	uint16 lastMid = 0;
	uint16 MAKThreshold;
	local_member_t *member;

	rx_handler_t *rxHandler;
	
	srcComp = findForeignComponent(srcCID);
	if (!srcComp)
	{
		;//syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: Not tracking this component");
		return wrapper->length;
	}
	remoteLeader = findLeader(srcComp);
	if (!remoteLeader)
	{
		;//syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: Source component is not a leader");
		return -1;
	}
		
	channelNumber = unmarshalU16(wrapper->data + processed);
	processed += sizeof(uint16);
	remoteChannel = findChannel(remoteLeader, channelNumber);
	
	if (!remoteChannel)
	{
		;//syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: Channel not found");
		return -1;
	}
	
	totalSeq = unmarshalU32(wrapper->data + processed);
	processed += sizeof(uint32);
	
	reliableSeq = unmarshalU32(wrapper->data + processed);
	processed += sizeof(uint32);
	
	oldestAvail = unmarshalU32(wrapper->data + processed);
	processed += sizeof(uint32);

	firstMid = unmarshalU16(wrapper->data + processed);
	processed += sizeof(uint16);

	lastMid = unmarshalU16(wrapper->data + processed);
	processed += sizeof(uint16);
	
	MAKThreshold = unmarshalU16(wrapper->data + processed);
	processed += sizeof(uint16);
/*	

	packetCount++;
	
	if (packetCount == 5)
	{
		syslog(LOG_LOCAL1 | LOG_WARNING, "sdtRxWrapper: Generating bad total - was %d", totalSeq);
		totalSeq++;
	}
	if (packetCount == 8)
	{
		syslog(LOG_LOCAL1 | LOG_WARNING, "sdtRxWrapper: Generating bad reliable was %d", reliableSeq);
		totalSeq++;
		reliableSeq++;
	}
*/
	//check the sequencing
	switch(checkSequence(remoteChannel, (wrapper->vector == SDT_REL_WRAPPER), totalSeq, reliableSeq, oldestAvail))
	{
		case SEQ_VALID :
			;//syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: Sequence correct");
			break; //continue on with this packet
		case SEQ_NAK :
			//initiate nak processing
			syslog(LOG_LOCAL1 | LOG_WARNING, "sdtRxWrapper: Missing wrapper(s) detected - Sending NAK");
			member = remoteChannel->memberList;
			if (member)
				sdtTxNak(remoteLeader, remoteChannel, member, reliableSeq);
			return wrapper->length;
			break;
		case SEQ_OLD_PACKET :
			syslog(LOG_LOCAL1 | LOG_INFO, "sdtRxWrapper: Old (Out of sequence) wrapper detected - Discarding");
			return wrapper->length;
			//Discard
			break;
		case SEQ_DUPLICATE :
			syslog(LOG_LOCAL1 | LOG_INFO, "sdtRxWrapper: Duplicate wrapper detected - Discarding");
			return wrapper->length;			
			break;
		case SEQ_LOST : 
			syslog(LOG_LOCAL1 | LOG_INFO, "sdtRxWrapper: Lost Reliable Wrappers not available - LOST SEQUENCE");
			//Discard and send sequence lost message to leader
			member = remoteChannel->memberList;
			if (member)
				sdtTxLeaving(remoteLeader, remoteChannel, member, SDT_LEAVE_LOST_SEQUENCE);
			return wrapper->length;
			break;
	}
	member = (local_member_t*)remoteChannel->memberList;
	//FIXME -- this only works with one cid at this IP!	
	rlpFormatPacket(member->component->cid, PROTO_SDT);

	
	if (member->pending || ((member->mid >= firstMid) && (member->mid <= lastMid))) //Currently doesn't care about the MAK Threshold
	{
		sdtFormatWrapper(0, member->component, 0xFFFF, 0xFFFF, 0);
		member->pending = 0;
		sdtTxAck(srcComp, remoteChannel);
	}
	else
		sdtFormatWrapper(1, member->component, 0xFFFF, 0xFFFF, 0);

	member->expiresAt = timer_set(member->expiryTime); //reset the timeout
	//decode and dispatch the wrapped pdus
	memset(&clientPdu, 0, sizeof(pdu_header_type));
	while(processed < wrapper->dataLength)
	{
		member = (local_member_t*)remoteChannel->memberList;
		processed += decodePdu(wrapper->data + processed, &clientPdu, sizeof(uint16), 6); //vectors are MIDs, Header contains 6 bytes
		while(member)
		{
			if ((clientPdu.vector == 0xFFF) || (member->mid == clientPdu.vector))
			{
				clientProtocol = unmarshalU32(clientPdu.header);
				relatedChannel = unmarshalU16((clientPdu.header + 4));
				
				localLeader = findLeader(member->component);
				if (!localLeader)
				{
					syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: local component is not a leader");
					return -1;
				}
				if (relatedChannel)
				{
					localChannel = findChannel(localLeader, relatedChannel);

					if (!localChannel)
					{
						syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxWrapper: local Channel not found");
						return -1;
					}
				}
				else
				{
					localChannel = 0;
				}
				
				if (clientProtocol == PROTO_SDT)
				{
				    sdtClientRxHandler(remoteLeader, remoteChannel, member, localLeader, localChannel, clientPdu.data, clientPdu.dataLength);
				} else {
				     rxHandler = getRXHandler(clientProtocol,handlers)    
				     if (!rxHandler) {
				           syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxWrapper: Unknown Vector - skip");
				      } else {
				           rxHandler(remoteLeader->component,member->component,remoteChannel, clientPdu.data, clientPdu.dataLength);
				      }				
				}
			}
			member = member->next;
		}
	}
	sdtTxCurrentWrapper();
//	syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxWrapper:Complete");
	return processed;
}

static void sdtClientRxHandler(leader_t *remoteLeader, channel_t *remoteChannel, local_member_t *member, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength)
{
	pdu_header_type pdu;
	uint32 processed = 0;
	foreign_member_t * remoteMember;
	
	memset(&pdu, 0, sizeof(pdu_header_type));
	//decode and dispatch the wrapped pdus
//	syslog(LOG_DEBUG | LOG_LOCAL1,"sdtClientRx Begin");
	while(processed < dataLength)
	{
		processed += decodePdu(data, &pdu, sizeof(uint8), 0); //single byte vectors and no headers
		switch(pdu.vector)
		{
			case SDT_ACK :
				remoteMember = findMemberByComponent(localChannel, remoteLeader->component);
				if (!remoteMember)
				{
					syslog(LOG_LOCAL1 | LOG_ERR,"sdtRxWrapper: Can't find remoteMember");
					return;
				}
				sdtRxAck(remoteMember, pdu.data, pdu.dataLength);
				break;
			case SDT_CHANNEL_PARAMS :
				break;
			case SDT_LEAVE :
				sdtRxLeave(remoteLeader, remoteChannel, localLeader, localChannel, pdu.data, pdu.dataLength);
				break;
			case SDT_CONNECT :
				sdtRxConnect(remoteLeader, remoteChannel, localLeader, localChannel, pdu.data, pdu.dataLength);
				break;
			case SDT_CONNECT_ACCEPT :
				remoteMember = findMemberByComponent(localChannel, remoteLeader->component);
				if (!remoteMember)
				{
					syslog(LOG_LOCAL1 | LOG_ERR,"sdtRxWrapper: Can't find remoteMember");
					return;
				}
				sdtRxConnectAccept(remoteMember, pdu.data, pdu.dataLength);
				break;
			case SDT_CONNECT_REFUSE :
				syslog(LOG_LOCAL1 | LOG_DEBUG,"sdtRxClientRxHandler: Our Connect was Refused ??? ");
				break;
			case SDT_DISCONNECT :
				syslog(LOG_LOCAL1 | LOG_DEBUG,"sdtRxClientRxHandler: rx sdt DISCONNECT (NOP)");
				break;
			case SDT_DISCONNECTING :
				syslog(LOG_LOCAL1 | LOG_DEBUG,"sdtRxClientRxHandler: rx sdt DISCONNECTING (isn't that special)");
				break;
			default :
				syslog(LOG_DEBUG|LOG_LOCAL1,"sdtClientRxHandler: Unknown Vector -- Skipping");
		}
	}
//	syslog(LOG_DEBUG | LOG_LOCAL1,"sdtClientRx End");
}


/*
static void sdtTxLeave(leader_t *leader, channel_t *channel, foreign_member_t *member)
{
	pdu_header_type leaveHeader;
	
	leaveHeader.vector = SDT_LEAVE;
	leaveHeader.header = 0;
	leaveHeader.headerLength = 0;
	leaveHeader.data = 0;
	leaveHeader.dataLength = 0;
	enqueueClientBlock(leader->component, member->component, &leaveHeader);
	txQueuedBlocks(PROTO_SDT, 0, member->mid, member->mid);
}
*/

static void sdtRxConnectAccept(foreign_member_t *member, uint8 *data, int dataLength)
{
	uint32 protocol;
	
	if (dataLength != 4)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxConnectAccept: invalid datalen");
		return;
	}
	syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxConnectAccept");
	protocol = unmarshalU32(data);
	if (protocol == PROTO_DMP)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxConnectAccept: WooHoo pointless Session established");
		member->isConnected = 1;
	}
}

static void sdtTxConnectAccept(foreign_component_t *dstComp, uint32 protocol, channel_t *foreignChannel)
{
	uint8 *buf;
	
	buf = sdtFormatClientBlock(dstComp, PROTO_SDT, foreignChannel);
	buf += marshalU16(buf, 7 | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buf += marshalU8(buf, SDT_CONNECT_ACCEPT);
	buf += marshalU32(buf, protocol);
	enqueueClientBlock(7);
}

static void sdtRxConnect(leader_t *remoteLeader, channel_t *remoteChannel, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength)
{
	uint32 protocol;
	
	if (dataLength != 4)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxConnect: invalid datalen");
		return;
	}
	syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxConnect");
	protocol = unmarshalU32(data);
	if (protocol == PROTO_DMP)
	{
		sdtTxConnectAccept(remoteLeader->component, protocol, remoteChannel);
		sdtTxConnect(localLeader, localChannel, remoteLeader->component, remoteChannel, PROTO_DMP);
	}
}

static void sdtTxConnect(leader_t *localLeader, channel_t *localChannel, foreign_component_t *dstComp, channel_t *foreignChannel, uint32 protocol)
{
	uint8 *buf;
	
	buf = sdtFormatClientBlock(dstComp, PROTO_SDT, 0);
	buf += marshalU16(buf, 7 | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buf += marshalU8(buf, SDT_CONNECT);
	buf += marshalU32(buf, protocol);
	enqueueClientBlock(7);
}


static void sdtRxLeave(leader_t *remoteLeader, channel_t *remoteChannel, leader_t *localLeader, channel_t *localChannel, uint8 *data, int dataLength)
{
//	syslog(LOG_LOCAL1 | LOG_DEBUG, "rxLeave Not dead yet");
	local_member_t *member = findMemberByComponent(remoteChannel, localLeader->component);
	
	sdtTxLeaving(remoteLeader, remoteChannel, member, SDT_LEAVE_REQUESTED);
	member->expiresAt = jiffies; //expire the member
//	syslog(LOG_LOCAL1 | LOG_DEBUG, "rxLeave I'm feeling better");
}

static void sdtTxLeaving(leader_t *leader, channel_t *channel, local_member_t *member, uint8 reason)
{
	uint8 *buffer = rlpFormatPacket(member->component->cid, PROTO_SDT);
	
	buffer += marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buffer += marshalU8(buffer, SDT_LEAVING);
	buffer += marshalCID(buffer, ((foreign_component_t*)leader->component)->cid);
	buffer += marshalU16(buffer, channel->channelNumber);
	buffer += marshalU16(buffer, member->mid);
	buffer += marshalU32(buffer, channel->reliableSeq);
	buffer += marshalU8(buffer, reason);
	rlpEnqueue(28);
	rlpSendTo(channel->sock, ((foreign_component_t*)leader->component)->adhocIP, ((foreign_component_t*)leader->component)->adhocPort, 0);
}


static void sdtTxDisconnect(leader_t *leader, channel_t *channel, local_member_t *member, uint32 protocol)
{
	uint8 *buf;
	rlpFormatPacket(((local_component_t*)leader->component)->cid, PROTO_SDT);
	sdtFormatWrapper(1, leader->component, 0xFFFF, 0xFFFF, 0);
	
	buf = sdtFormatClientBlock((foreign_component_t*)((member) ? member->component : 0), PROTO_SDT, 0);
	buf += marshalU16(buf, 7 | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buf += marshalU8(buf, SDT_DISCONNECT);
	buf += marshalU32(buf, protocol);
	enqueueClientBlock(7);
	sdtFlush();
	if (member)
	{
		member->isConnected = 0;
	}
	else
	{
		member = channel->memberList;
		while(member)
		{
			member->isConnected = 0;
			member = member->next;
		}
	}
}

static void sdtTxLeave(leader_t *leader, channel_t *channel, local_member_t *member)
{
	syslog(LOG_DEBUG|LOG_LOCAL1,"sdtTxLeave");
}

static void sdtTxNak(leader_t *leader, channel_t *channel, local_member_t *member, uint32 lastMissed)
{
	//FIXME -  add proper nak storm suppression
	uint8 *buffer = rlpFormatPacket(member->component->cid, PROTO_SDT);
	
	buffer += marshalU16(buffer, 35 /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buffer += marshalU8(buffer, SDT_NAK);
	buffer += marshalCID(buffer, ((foreign_component_t*)leader->component)->cid);
	buffer += marshalU16(buffer, channel->channelNumber);
	buffer += marshalU16(buffer, member->mid);
	buffer += marshalU32(buffer, channel->reliableSeq);
	buffer += marshalU32(buffer, channel->reliableSeq + 1);
	buffer += marshalU32(buffer, lastMissed);
	rlpEnqueue(35);
		rlpSendTo(channel->sock, ((foreign_component_t*)leader->component)->adhocIP, ((foreign_component_t*)leader->component)->adhocPort, 0);
}

//returns SEQ_VALID if packet is correctly sequenced
static inline uint32 checkSequence(channel_t *s, uint8 isReliable, uint32 rxTotal, uint32 rxReliable, uint32 oldestAvail)
{
	int diff;
	
	diff = rxTotal - s->totalSeq;
//	syslog(LOG_LOCAL1 | LOG_WARNING, "sdtCheckSequence: Rel?=%d, s->tot=%d, s->rel=%d, rxTot=%d, rxRel=%d, rxOldAvail=%d",
//				isReliable, s->totalSeq, s->reliableSeq, rxTotal, rxReliable, oldestAvail);
	if (diff == 1) //normal seq
	{
//		syslog(LOG_LOCAL1 | LOG_WARNING, "sdtCheckSequence: normalSeq"); 
		//update channel object
		s->totalSeq = rxTotal;
		s->reliableSeq = rxReliable;
		return SEQ_VALID; //packet should be processed
	}
	else if (diff > 1) //missing packet(s) check reliable
   	{
		diff = rxReliable - s->reliableSeq;
		if ((isReliable) && (diff == 1))
		{
//			syslog(LOG_LOCAL1 | LOG_WARNING, "sdtCheckSequence: missing unreliable");
			s->totalSeq = rxTotal;
			s->reliableSeq = rxReliable;
			return SEQ_VALID;
		}
		else if (diff)
		{
			diff = oldestAvail - s->reliableSeq;
			if (diff > 1)
				return SEQ_LOST; 
			else
				return SEQ_NAK;//activate nak system
		}
		else //missed unreliable packets
		{
			s->totalSeq = rxTotal;
			s->reliableSeq = rxReliable;
			return SEQ_VALID;
		}
	}
	else if (diff == 0) //old packet or duplicate - discard
		return SEQ_DUPLICATE;
	//else diff < 0
   	return SEQ_OLD_PACKET;
}

enum
{
	ALLOCATED_FOREIGN_COMP = 1,
	ALLOCATED_FOREIGN_LEADER = 2,
	ALLOCATED_FOREIGN_CHANNEL = 4,
	ALLOCATED_LOCAL_MEMBER = 8,
	ALLOCATED_LOCAL_LEADER = 16,
	ALLOCATED_LOCAL_CHANNEL = 32,
	ALLOCATED_FOREIGN_MEMBER = 64,
};


struct PACKED joinMsg_s {
	cid_t cid;
	uint16_t n_mid;
	uint16_t n_channel;
	uint16_t n_reciprocal;
	uint32_t n_seq
};

//static uint32 sdtRxJoin(udp_transport_t *transportAddr, uint8 srcCID[16], pdu_header_type *header)

static int sdtRxJoin(
	const uint8_t *data,
	int datasize,
	void *ref,
	const netiHost_t *remhost,
	const cid_t *remcid
)
{
//	char srcText[40];
//	char dstText[40];
	uint8 dstCID[16];
	uint16 foreignChannelNumber;
	uint16 localChannelNumber;
	uint32 processed = 0;
	local_component_t *localComp;
	foreign_component_t *remComp;
	leader_t *localLeader;
	leader_t *foreignLeader;
	channel_t *localChannel;
	channel_t *foreignChannel;
	local_member_t *localMember;
	uint16 mid;
	int addressType;
	int allocations = 0;

	syslog(LOG_USER |LOG_INFO, "sdtRxJoin");
	if (datasize < 30)
	{
		syslog(LOG_USER |LOG_ERR, "sdtRxJoin - pdu data too short");
		return 1-;
	}
	
	//	memcpy(dstCID, header->data, sizeof(uuid_type)) //leave till we are sure we want it
	//processed += sizeof(uuid_type);
	
	localComp = findLocalComponent((cid_t *)(data + mJOIN_CID));
	if (!localComp)
	{
		syslog(LOG_USER |LOG_INFO, "sdtRxJoin -- Not addressed to me");
		return 0; //not addressed to any local component
	}
	
	mid = unmarshalU16(data + mJOIN_MID);
	foreignChannelNumber = unmarshalU16(data + mJOIN_CHANNEL);
	localChannelNumber = unmarshalU16(data + mJOIN_RECIPROCAL); //reciprocal Channel 
	
	//Am I tracking the src component
	remComp = findForeignComponent(remcid);
	if (!remComp)
	{
		remComp = registerForeignComponent(remcid);
		if (!remComp) //allocation failure
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to register remComp");
			return -1;
			//FIXME -- Handle error - txRefuse
			//send a refuse resources
		}
		else
		{
			allocations |= ALLOCATED_FOREIGN_COMP;
		}	
	}

	memcpy(&remComp->adhocNetaddr, remhost, sizeof(*remhost));
//	remComp->adhocIP = remhost->srcIP;
//	remComp->adhocPort = transportAddr->srcPort;
	
	//Do I have information on this channelLeader
	foreignLeader = findLeader(remComp);
	if (!foreignLeader)
	{
		foreignLeader = addLeader(remComp);
		if (!foreignLeader) //allocation failure
		{
		return -1;
			;//syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to register remComp as leader");
			//FIXME -- Handle error - txRefuse
			//send a refuse resources
		}
		else
		{
			allocations |= ALLOCATED_FOREIGN_LEADER;
		}
	}

	//Do I have information on this channelLeader
	localLeader = findLeader(localComp);
	if (!localLeader)
	{
		localLeader = addLeader(localComp);
		if (!localLeader) //allocation failure
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to register localComp as leader");
			return 1;
			//FIXME -- Handle error - txRefuse
			//send a refuse resources
		}
		else
		{
			allocations |= ALLOCATED_LOCAL_LEADER;
		}
	}
	
	//Do I have information on this channel
	foreignChannel = foreignLeader->channelList;    //findChannel(foreignLeader, foreignChannelNumber);
	if (!foreignChannel)
	{
		foreignChannel = addChannel(foreignLeader, foreignChannelNumber);
		if (!foreignChannel) //allocation failure
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to add channel to leader");
			return 1;
			//FIXME -- Handle error - txRefuse
			//send a refuse resources
		}
		else
		{
			allocations |= ALLOCATED_FOREIGN_CHANNEL;
		}
	}

	//Check if I am already a member
	localMember = findMemberByComponent(foreignChannel, localComp);
	if (!localMember)
	{
		localMember = addLocalMember(foreignChannel);
		if (!localMember) //allocation failure
		{
			syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to allocate member object");
			return 1;			//FIXME -- Handle error - txRefuse
		}
		else
		{
			allocations |= ALLOCATED_LOCAL_MEMBER;
		}
	}
	else
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : Already joined channel from this source");
		sdtTxJoinRefuse(transportAddr, srcCID, header, SDT_REFUSE_NO_UPSTREAM_CHAN);
		return 1;
	}
		
	if (localChannelNumber == 0) //this is a foreign comp initiated join
	{
		localChannel = localLeader->channelList;
		if (!localChannel)
		{
			localChannel = addChannel(localLeader, ((ticks & 0xffff) | 0x0001));
			if (!localChannel)
			{
				syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxJoin : failed to allocate local channel");
				//FIXME -- Handle error - Back out 
				//send a refuse resources
			}
			else
			{
				localChannel->isLocal = 1;
				localChannel->availableMid = 1;
				localChannel->totalSeq = 1;
				localChannel->reliableSeq = 1;
				localChannel->oldestAvail = 1;
				localChannel->downstreamPort = LOCAL_ADHOC_PORT; //only open 1 local port by choice
				localChannel->downstreamIP = mcast_alloc_new(localComp); //FIXME -- should use proper mcast alloc
				localChannel->sock = rlpOpenSocket(localChannel->downstreamPort);
				mcast_join(localChannel->downstreamIP);	// DEPENDS WATERLOO
				allocations |= ALLOCATED_LOCAL_CHANNEL;
			}
		}
	}
	else //this is a local comp initiated join
	{
		; //we don't do this yet
	}

	//fill in the channel structure
	foreignChannel->totalSeq = unmarshalU32(header->data + processed);
	processed += sizeof(uint32);
	foreignChannel->reliableSeq = unmarshalU32(header->data + processed);
	processed += sizeof(uint32);
	
	processed += unmarshalTransportAddress(header->data + processed, 0, &(foreignChannel->downstreamPort), &(foreignChannel->downstreamIP), &addressType);
	if (addressType == SDT_ADDR_IPV6)
	{
			syslog(LOG_LOCAL1 | LOG_WARNING, "sdtRxJoin : Unsupported downstream Address Type");
			return 1;
			//send refuse -- reason ADDR_TYPE - txRefuse
			//FIXME --handle error - txRefuse
	}


	//fill in the member structure
	localMember->component = localComp;
	localMember->mid = mid;
	processed += unmarshalChannelParamBlock(localMember, header->data + processed);
	
	if (allocations & ALLOCATED_FOREIGN_CHANNEL)
	{
		foreignChannel->sock = rlpOpenSocket(foreignChannel->downstreamPort);
		mcast_join(foreignChannel->downstreamIP);	// DEPENDS WATERLOO
	}
	localMember->pending=1;
	localMember->expiresAt = timer_set(localMember->expiryTime);
	sdtTxJoinAccept(remComp, foreignChannel, localMember, localChannel);
	sdtTxJoin(localComp, localChannel, remComp, foreignChannel);
	;//syslog(LOG_LOCAL1 | LOG_INFO,;//syslogBuf);
	return processed;
}


static void sdtTxJoin(local_component_t *lComp, channel_t *lChannel, foreign_component_t *fComp, channel_t *fChannel)
{
	uint32 allocations = 0;
	foreign_member_t *fMember;
	uint8 *buffer = rlpFormatPacket(lComp->cid, PROTO_SDT);
	
	fMember = findMemberByComponent(lChannel, fComp);
	if (fMember)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtTxJoin : already a member -- suppressing join");
		return; //Dont add them twice.
	}	

	fMember = addForeignMember(lChannel);
	if (!fMember)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtTxJoin : failed to allocate foreign member");
		//FIXME -- Handle error - Back out 
		//send a refuse resources
	}
	else
	{
		if (lChannel->availableMid == 0x7FFF)
			lChannel->availableMid = 1;
		while(findMemberByMid(lChannel, lChannel->availableMid)) //handles the rollover causing an overlap with an existing member
			lChannel->availableMid++;
		fMember->component = fComp;
		fMember->expiresAt = timer_set(FOREIGN_MEMBER_EXPIRY_TIME);
		fMember->mid = lChannel->availableMid++;
		allocations |= ALLOCATED_FOREIGN_MEMBER;
	}
	
	buffer += marshalU16(buffer, 49 /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buffer += marshalU8(buffer, SDT_JOIN);
	buffer += marshalCID(buffer, fComp->cid);

	buffer += marshalU16(buffer, fMember->mid);
	buffer += marshalU16(buffer, lChannel->channelNumber);
	buffer += marshalU16(buffer, fChannel->channelNumber); //Reciprocal Channel
	buffer += marshalU32(buffer, lChannel->totalSeq);
	buffer += marshalU32(buffer, lChannel->reliableSeq);
	
	buffer += marshalTransportAddress(buffer, lChannel->downstreamPort, lChannel->downstreamIP, SDT_ADDR_IPV4);
	buffer += marshalChannelParamBlock(buffer);
	buffer += marshalU8(buffer, 255); //My Adhoc Port does not change
	
	rlpEnqueue(49);
	rlpSendTo(lChannel->sock, fComp->adhocIP, fComp->adhocPort, 0);
}


static void sdtTxJoinAccept(foreign_component_t *fComp, channel_t *fChannel, local_member_t *lMember, channel_t *lChannel)
{
	uint8 *buffer = rlpFormatPacket(lMember->component->cid, PROTO_SDT);
	
	buffer += marshalU16(buffer, 29 /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buffer += marshalU8(buffer, SDT_JOIN_ACCEPT);
	buffer += marshalCID(buffer, fComp->cid);
	buffer += marshalU16(buffer, fChannel->channelNumber);
	buffer += marshalU16(buffer, lMember->mid);
	buffer += marshalU32(buffer, fChannel->reliableSeq);
	buffer += marshalU16(buffer, lChannel->channelNumber);
	rlpEnqueue(29);
	
	rlpSendTo(fChannel->sock, fComp->adhocIP, fComp->adhocPort, 0);
}

static void sdtTxJoinRefuse(udp_transport_t *transportAddr, uint8 cid[16], pdu_header_type *joinRequest, uint8 reason)
{
	uint8 *buffer = rlpFormatPacket(cid, PROTO_SDT);
	
	buffer += marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_wFLAG | HEADER_wFLAG | DATA_wFLAG);
	buffer += marshalU8(buffer, SDT_JOIN_REFUSE);
	buffer += marshalCID(buffer, cid);

	buffer += marshalU16(buffer, unmarshalU16(joinRequest->data + 18)); //leader's channel #
	buffer += marshalU16(buffer, unmarshalU16(joinRequest->data + 16)); //mid that the leader was assigning to me
	buffer += marshalU32(buffer, unmarshalU32(joinRequest->data + 26)); //leader's channel rel seq #
	buffer += marshalU8(buffer, reason);
	rlpEnqueue(28);
	
	rlpSendTo(rlpGetSocket(LOCAL_ADHOC_PORT), transportAddr->srcIP, transportAddr->srcPort, 0);
}


static uint32 sdtRxJoinAccept(uint8 *srcCID, pdu_header_type *header)
{
//	foreign_component_t *fComp;
//	local_component_t *lComp;
//	channel_t *fChanel;
	
	syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxJoinAccept");
	
//	fComp = findForeignComponent(srcCID);
//	lComp = 
//	
//	sdtFormatWrapper(0, 
	return 1;
}

static uint32 sdtRxJoinRefuse(uint8 *srcCID, pdu_header_type *header)
{
	syslog(LOG_DEBUG|LOG_LOCAL1,"sdtRxJoinRefuse");
	return (headerPresent(header->flags)) ? 24 : 0 + (dataPresent(header->flags)) ? 1 : 0;

}

static void sdtRxAck(foreign_member_t *remoteMember, uint8 *data, int dataLength)
{
	uint32 reliableSeq;
	
	syslog(LOG_LOCAL1 | LOG_INFO, "sdtRxAck");
	if (dataLength != 4)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxAck: Not enough bytes in the data");
		return;
	}
	reliableSeq = unmarshalU32(data);
	
	remoteMember->expiresAt = timer_set(FOREIGN_MEMBER_EXPIRY_TIME);
	
	return;
}

static void sdtRxNak(uint8 *srcCID, pdu_header_type *header)
{
	uint32 dstCID[4];
	
	short channelNum;
	leader_t *leader;
	local_component_t *comp;
	channel_t *channel;
	int offset = 0;
	int firstMissed;
	int lastMissed;
	int numBack;
	
	if (header->dataLength != 32)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxNak: Not enough bytes in the data");
		return;
	}
	
	offset += unmarshalCID(header->data + offset, (uint8 *)dstCID);
	
	comp = findLocalComponent((void*)dstCID);
	if (!comp)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxNak: No such local Component");
		return;
	}
	leader = findLeader(comp);
	if (!leader)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxNak: No such local leader");
		return;
	}
	
	channelNum = unmarshalU16(header->data + offset);
	offset += sizeof(uint16);
	channel = findChannel(leader, channelNum);
	if (!channel)
	{
		syslog(LOG_LOCAL1 | LOG_ERR, "sdtRxNak: No such Channel");
		return;
	}
	//The next field is the MID - this is not used - throw away
	offset += sizeof(uint16);
	//The next field is the member's Ack point - this is not used - throw away
	offset += sizeof(uint32);
	
	firstMissed = unmarshalU32(header->data + offset);
	offset += sizeof(uint32);
	numBack = channel->reliableSeq - firstMissed;
	if (firstMissed < channel->oldestAvail)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxNak: Requested Wrappers not avail");
		return;
	}
	lastMissed = unmarshalU32(header->data + offset);
	offset += sizeof(uint32);
	while(firstMissed != lastMissed)
	{
		rlpResendTo(channel->sock, channel->downstreamIP, channel->downstreamPort, numBack);
		numBack--;
		firstMissed++;
	}
}


static int sdtRxLeaving(uint8 *srcCID, pdu_header_type *header)
{
	uint32 cid[4];
	leader_t *lLeader;
	leader_t *fLeader;
	foreign_component_t *fComp;
	local_component_t *lComp;
	foreign_member_t *member;

	syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxLeaving: begin");

	fComp = findForeignComponent((void*)srcCID);
	if (!fComp)
		return 1;
	unmarshalCID(header->data, (uint8 *)cid);
	lComp = findLocalComponent((void*)cid);
	lLeader = findLeader(lComp);
	fLeader = findLeader(fComp);

	int channel_number = unmarshalU16(header->data + 16);
	channel_t *channel = findChannel(lLeader, channel_number);

	if (!channel)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxLeaving: localChannel not found ");	
		return 1;
	}
	member = findMemberByComponent(channel, fComp);
	if (!member)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxLeaving: member not found");
		return 2;
	}
		
	member->expiresAt = jiffies;
	dmpCompOfflineNotify(fComp);
	
//	removeForeignMember(channel, findMemberByComponent(channel, fComp));
/*	if (!channel->memberList)
	{
		syslog(LOG_LOCAL1 | LOG_DEBUG, "sdtRxLeaving: remove the local channel");			
		removeChannel(lLeader, channel);
	}
*/
	return 1;
}

// utility Functions

static inline leader_t *findLeader(void *component)
{
	leader_t *leader;
	
	leader = leaders;
	while(leader)
	{
		if (leader->component == component)
			break;
		leader = leader->next;
	}
	return leader;
}

static inline leader_t *addLeader(void *component)
{
	leader_t *leader;
	
	leader = ppMalloc(sizeof(leader_t));
	if (!leader) //out of memory
		return 0;

	leader->component = component;
	leader->next = leaders;
	leaders = leader;
	return leader;
}

static inline channel_t *findChannel(leader_t *leader, uint16 channelNumber)
{
	channel_t *channel;
	
	channel = leader->channelList;
	while(channel)
	{
		if (channel->channelNumber == channelNumber)
			break;
		channel = channel->next;
	}
	return channel;
}

static inline channel_t *addChannel(leader_t *leader, uint16 channelNumber)
{
	channel_t *channel;
	
	channel = ppMalloc(sizeof(channel_t));
	if (!channel) //out of memory
		return 0;

	channel->isLocal = 0;
	channel->next = leader->channelList;
	leader->channelList = channel;
	channel->channelNumber = channelNumber;
	channel->memberList = 0;
	return channel;
}

static inline void removeChannel(leader_t *leader,  channel_t *channel)
{
	channel_t **prev;
	channel_t *cur;
	
	prev = &(leader->channelList);
	cur = leader->channelList;
	while(cur)
	{
		if (cur == channel)
		{
			*prev = cur->next;
			ppFree(channel, sizeof(channel_t));
			break;
		}
		prev = &(cur->next);
		cur = cur->next;
	}
}

static inline void removeLeader(leader_t *leader)
{
	leader_t **prev;
	leader_t *cur;
	
	prev = &(leaders);
	cur = leaders;
	
	while(cur)
	{
		if (cur == leader)
		{
			*prev = cur->next;
			ppFree(leader, sizeof(leader_t));
			break;
		}
		prev = &(cur->next);
		cur = cur->next;
	}
}

static inline void *findMemberByMid(channel_t *channel, uint16 mid)
{
	local_member_t *member;
	
	member = channel->memberList;
	
	while(member)
	{
		if (member->mid == mid)
			break;
		member = member->next;
	}
	return member;
}

static inline local_member_t *addLocalMember(channel_t *channel)
{
	local_member_t *member;
	
	member = ppMalloc(sizeof(local_member_t));
	if (!member) //out of memory
		return 0;
		
	member->next = channel->memberList;
	channel->memberList = member;
	member->isLocal = 1;
	return member;
}

static inline void removeLocalMember(channel_t *channel, local_member_t *member)
{
	local_member_t **prev;
	local_member_t *cur;
	
	prev = (local_member_t**)&(channel->memberList);
	cur = (local_member_t*)channel->memberList;
	while(cur)
	{
		if (cur == member)
		{
			*prev = cur->next;
			ppFree(member, sizeof(local_member_t));
			break;
		}
		prev = &(cur->next);
		cur = cur->next;
	}
}

static inline void removeForeignMember(channel_t *channel, foreign_member_t *member)
{
	foreign_member_t **prev;
	foreign_member_t *cur;
	
	prev = (foreign_member_t**)&(channel->memberList);
	cur = (foreign_member_t*)channel->memberList;
	while(cur)
	{
		if (cur == member)
		{
			*prev = cur->next;
			ppFree(member, sizeof(foreign_member_t));
			break;
		}
		prev = &(cur->next);
		cur = cur->next;
	}
}


static inline foreign_member_t *addForeignMember(channel_t *channel)
{
	foreign_member_t *member;
	
	member = ppMalloc(sizeof(foreign_member_t));
	if (!member) //out of memory
		return 0;
		
	member->next = (void *)channel->memberList;
	channel->memberList = (void *)member;
	member->isLocal = 0;
	return member;
}

static inline void *findMemberByComponent(channel_t *channel, void *component)
{
	local_member_t *member;
	
	member = channel->memberList;
	
	while(member)
	{
		if (member->component == component)
			break;
		member = member->next;
	}
	
	return member;
}

//currently these parameters are hard coded - if this changes this function will need more arguments
static inline int marshalChannelParamBlock(uint8 *paramBlock)
{
	int offset = 0;
	
	offset += marshalU8(paramBlock + offset, FOREIGN_MEMBER_EXPIRY_TIME);
	offset += marshalU8(paramBlock + offset, 0); //suppress OUTBOUND NAKs from foreign members
	offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_HOLDOFF);
	offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_MODULUS);
	offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_MAX_WAIT);
	
	return offset;
}


static inline int unmarshalChannelParamBlock(local_member_t *member, uint8 *paramBlock)
{
	member->expiryTime = *paramBlock;
	paramBlock++;
	
	member->nak = *paramBlock;
	paramBlock++;
	member->nakHoldoff = unmarshalU16(paramBlock);
	paramBlock += sizeof(uint16);
	member->nakModulus = unmarshalU16(paramBlock);
	paramBlock += sizeof(uint16);
	member->nakMaxWait = unmarshalU16(paramBlock);
	paramBlock += sizeof(uint16);
	return 8; //length of the ParamaterBlock
}

static inline int marshalTransportAddress(uint8 *data, uint16 port, uint32 ipAddress, int type)
{
	int offset = 0;
	switch(type)
	{
		case SDT_ADDR_IPV4 :
			offset += marshalU8(data + offset, SDT_ADDR_IPV4);
			offset += marshalU16(data + offset, port);
			offset += marshalU32(data + offset, ipAddress);
			break;
	}
	return offset;
}


static inline int unmarshalTransportAddress(uint8 *data, udp_transport_t *transportAddr, uint16 *port, uint32 *ipAddress, int *type)
{
	int processed = 0;
	
	switch(*data)
	{
		case SDT_ADDR_NULL :
			processed += sizeof(uint8); //Address Type Byte
			
			*port = transportAddr->srcPort;
			*ipAddress = transportAddr->srcIP;
			*type = SDT_ADDR_NULL;
			break;
		case SDT_ADDR_IPV4 :
			processed += sizeof(uint8); //Address Type Byte
			*port = unmarshalU16(data + processed);
			processed += sizeof(uint16);
			*ipAddress = unmarshalU32(data + processed);
			processed += sizeof(uint32);
			break;
#if 0	//No longer in ACN spec
		case SDT_ADDR_IPPORT :
			processed += sizeof(uint8); //Address Type Byte
			*port = unmarshalU16(data + processed);
			*ipAddress = transportAddr->srcIP;
			break;
#endif
		case SDT_ADDR_IPV6 :
		default :
			processed += sizeof(uint8); //Address Type Byte
			;//syslog(LOG_LOCAL1 | LOG_WARNING, "sdtRxJoin : Unsupported upstream Address Type");
	}
	*type = *data;
	
	return processed;
}

static void sdtTick(void)
{
	leader_t *leader;
	leader_t **prevLeader;
	channel_t *channel;
	channel_t **prevChannel;
	
	foreign_member_t *member;
	foreign_member_t **prevMember;
	foreign_component_t *remoteComp;
	
	leader = leaders;
	prevLeader = &leaders;
	
	while(leader)
	{
		prevChannel = &(leader->channelList);
		channel = leader->channelList;
		while(channel)
		{
			//if the leader is local
			if (channel->isLocal)
			{
				//send a MAK to all members in my downstream channel
				sdtFormatWrapper(0, leader->component, 1, 0xFFFF, 0);
				sdtTxCurrentWrapper();
			}

			prevMember = (foreign_member_t**) &(channel->memberList);
			member = (foreign_member_t*)channel->memberList;
			while(member)
			{
				if (timer_expired(member->expiresAt))
				{
					if (member->isLocal)
					{
						//send a leaving message
						sdtTxLeaving(leader, channel, (local_member_t *)member, SDT_LEAVE_CHANNEL_EXPIRED);
					}
					else
//					{
						//command the member to leave my channel
						//sdtTxLeave(leader, channel, member);
						dmpCompOfflineNotify(member->component);
//					}
					//remove this member from the channel
					*prevMember = member->next;
					ppFree(member, (member->isLocal) ? sizeof(local_member_t) : sizeof(foreign_member_t));
					member = *prevMember;
				}
				else
				{
					prevMember = &(member->next);
					member = member->next;
				}
			}
			if (!channel->memberList) //no members left in the channel
			{
				if (currentChannel == channel)
				{
					currentSrc = 0;
					currentChannel = 0;
					currentLeader =0;
				}
				mcast_leave(channel->downstreamIP);
				rlpCloseSocket(channel->sock);
				//remove the channel from the leader
				*prevChannel = channel->next;
				ppFree(channel, sizeof(channel_t));
				channel = *prevChannel;
			}
			else
			{
				prevChannel = &(channel->next);
				channel = channel->next;
			}
		}
		if (!leader->channelList) //no channels left on this leader
		{
			remoteComp = leader->component;
			//remove the leader
			*prevLeader = leader->next;
			ppFree(leader, sizeof(leader_t));
			leader = *prevLeader;
			unregisterForeignComponent(remoteComp->cid);
		}
		else
		{
			prevLeader = &(leader->next);
			leader = leader->next;
		}
	}
}

// End Utility Functions
rlpChannel_t *adhoc = NULL;

int initSdt(bool acceptAdHoc)
{
	static bool initializedState = 0;

	if (initialized) return 0;
	if (initRlp() < 0) return -1;

	// PN-FIXME must register SDT with RLP
	// PN-FIXME remove non-standard memory allocations
	/* FIXME register with SLP here */

	allocateToPool((void*)leaderMemory, sizeof(leader_t) * MAX_LEADERS, sizeof(leader_t));
	allocateToPool((void*)channelMemory, sizeof(channel_t) * MAX_CHANNELS, sizeof(channel_t));
	allocateToPool((void*)localMemberMemory, sizeof(local_member_t) * MAX_LOCAL_MEMBERS, sizeof(local_member_t));
	allocateToPool((void*)foreignMemberMemory, sizeof(foreign_member_t) * MAX_FOREIGN_MEMBERS, sizeof(foreign_member_t));

	leaders = 0;
	addTask(5000, sdtTick, -1);	// PN-FIXME

	if (acceptAdHoc)
	{
		/* Open ad hoc channel on ephemerql port */
		adhoc = rlpOpenChannel(NETI_PORT_NONE, NETI_INADDR_ANY, PROTO_SDT, &adhocRxHandler, adhocMsgTab);
		
		/* now need to register with SLP passing port getLocalPort(adhoc) */
	}

	initialized = 1;
	return 0;
}

struct localComponent_s {
	cid_t cid;
	acnProtocol_t protocols[MAXPROTOCOLS];
	int (*connectfilter)(cid_t, struct session_s sessionInfo, acnProtocol_t protocol);
};

int sdtRegister(struct localComponent_s comp)
{
	addLocalComponent(comp);
}

void shutdownSdt(void)
{
	leader_t *leader;
	channel_t *channel;
	
	foreign_member_t *member;
	
	leader = leaders;
	
	while(leader)
	{
		channel = leader->channelList;
		while(channel)
		{
			if (channel->isLocal)
			{
				//disconnect the session
				sdtTxDisconnect(leader, channel, 0, PROTO_DMP);
				//command all the members to leave my channel
				sdtTxLeave(leader, channel, 0);
			}
			else
			{
				member = (foreign_member_t*)channel->memberList;
				while(member)
				{
					//send a leaving message
					sdtTxLeaving(leader, channel, (local_member_t *)member, SDT_LEAVE_NONSPEC);
					member = member->next;
				}
			}
			channel = channel->next;
		}
		leader = leader->next;
	}
}
