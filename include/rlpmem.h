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

#ifndef __rlpmem_h__
#define __rlpmem_h__ 1

void rlpInitChannels(void);

void rlpInitSockets(void);
netSocket_t *rlpFindNetSock(port_t localPort);
netSocket_t *rlpNewNetSock(void);
void rlpFreeNetSock(netSocket_t *sockp);

void rlpInitChannels(void);
struct rlpChannelGroup_s *rlpNewChannelGroup(netSocket_t *netsock, netAddr_t groupAddr);
void rlpFreeChannelGroup(netSocket_t *netsock, struct rlpChannelGroup_s *channelgroup);
struct rlpChannelGroup_s *rlpFindChannelGroup(netSocket_t *netsock, netAddr_t groupAddr);
struct rlpChannel_s *rlpNewChannel(struct rlpChannelGroup_s *channelgroup);
void rlpFreeChannel(struct rlpChannelGroup_s *channelgroup, struct rlpChannel_s *channel);
struct rlpChannel_s *rlpFirstChannel(struct rlpChannelGroup_s *channelgroup, acnProtocol_t pduProtocol);
struct rlpChannel_s *rlpNextChannel(struct rlpChannelGroup_s *channelgroup, struct rlpChannel_s *channel, acnProtocol_t pduProtocol);
int sockHasGroups(netSocket_t *netsock);
int groupHasChannels(struct rlpChannelGroup_s *channelgroup);
struct rlpChannel_s *getChannelgroup(struct rlpChannel_s *channel);
netSocket_t *getNetsock(struct rlpChannelGroup_s *channelgroup);
void rlpInitBuffers(void);

typedef uint16_t usage_t;

#if defined(CONFIG_RLPMEM_DYNAMICBUF)
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
#endif
#define bufhdrp(buf) ((struct rlpTxbufhdr_s *)(buf))

#endif
