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
#ifndef __sdt_h__
#define __sdt_h__

#include <arch/types.h>
#include "pdu.h"
#include "rlp.h"
#include <acn_config.h>

#define  INITIAL_PORT			0xED02
#define	MAX_CHANNELS			50
#define	MAX_LEADERS				50
#define	MAX_LOCAL_MEMBERS		10
#define	MAX_FOREIGN_MEMBERS	50


#define FOREIGN_MEMBER_EXPIRY_TIME	15
#define FOREIGN_MEMBER_NAK_HOLDOFF	1
#define FOREIGN_MEMBER_NAK_MODULUS	1
#define FOREIGN_MEMBER_NAK_MAX_WAIT	5
enum
{
   SDT_REL_WRAPPER       = 1,
   SDT_UNREL_WRAPPER     = 2,
   SDT_CHANNEL_PARAMS    = 3,
   SDT_JOIN              = 4,
   SDT_JOIN_REFUSE       = 5,
   SDT_JOIN_ACCEPT       = 6,
   SDT_LEAVE             = 7,
   SDT_LEAVING           = 8,
	SDT_CONNECT				 = 9,
	SDT_CONNECT_ACCEPT	 = 10,
	SDT_CONNECT_REFUSE	 = 11,
	SDT_DISCONNECT			 = 12,
	SDT_DISCONNECTING		 = 13,
   SDT_ACK               = 14,
   SDT_NAK               = 15,
   SDT_GET_SESSIONS      = 16,
   SDT_SESSIONS          = 17, 
} ; //SDT pdu types

enum 
{
	SDT_REFUSE_NONSPEC			= 1,
	SDT_REFUSE_PARAMETERS		= 2,
	SDT_REFUSE_RESOURCES			= 3,
	SDT_REFUSE_ALREADY_MEMBER	= 4,
	SDT_REFUSE_TRANSPORT_ADDR	= 5,
	SDT_REFUSE_NO_UPSTREAM_CHAN= 6,	
};


enum 
{
	SDT_LEAVE_NONSPEC				= 1,
	SDT_LEAVE_CHANNEL_EXPIRED	= 2,
	SDT_LEAVE_LOST_SEQUENCE		= 3,
	SDT_LEAVE_SATURATED			= 4,
	SDT_LEAVE_REQUESTED			= 5,
};
enum 
{
	SDT_LEAVING_NONSPEC			= 1,
	SDT_LEAVING_EXPIRED			= 2,
	SDT_LEAVING_LOST_SEQUENCE	= 3,
	SDT_LEAVING_SATURATED		= 4,
	SDT_LEAVING_BY_REQUEST		= 5,
};

enum
{
	SDT_STATE_NON_MEMBER,
	SDT_STATE_JOIN_SENT,
	SDT_STATE_JOIN_ACCEPT_SENT,
	SDT_STATE_JOIN_RECEIVED,
	SDT_STATE_CHANNEL_OPEN,
	SDT_STATE_LEAVING,	
};

enum
{
   SDT_ADDR_NULL = 0,
   SDT_ADDR_IPV4 = 1,
   SDT_ADDR_IPV6 = 2,
	SDT_ADDR_IPPORT = 3,
   
};

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
#endif
