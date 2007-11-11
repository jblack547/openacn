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
#include "opt.h"
#include "types.h"
#include "acn_arch.h"
#include "netiface.h"
#include "rlp.h"

#define INPACKETSIZE DEFAULT_MTU

extern uint8_t *neti_newPacket(int size);

#if CONFIG_NET_IPV4

const int optionOn = 1;
const int optionOff = 0;

#if CONFIG_STACK_BSD

void neti_init(void)
{
	return;
}

int
neti_udp_open(struct netsocket_s *rlpsock, port_t localport)
{
	int bsdsock;
	int rslt;
	union {
		struct sockaddr gen;
		struct sockaddr_in ip4;
	} localaddr;

	bsdsock = socket(PF_INET, SOCK_DGRAM, 0);
	if (bsdsock < 0) return -errno;
    rslt = setsockopt (bsdsock, SOL_SOCKET, SO_REUSEADDR, (void *)&optionOn, sizeof(optionOn));
	if (rslt < 0) goto fail;

	localaddr.gen.sa_family = AF_INET;
	localaddr.ip4.sin_port = localport;
	localaddr.ip4.sin_addr.s_addr = INADDR_ANY;
	rslt = bind(bsdsock, &localaddr.gen, sizeof(struct sockaddr_in));
	if (rslt < 0) goto fail;

	rlpsock->nativesock = bsdsock;
	rlpsock->localport = localport;

	/* we need information on destination address used */
    rslt = setsockopt (bsdsock, IPPROTO_IP, IP_PKTINFO, (void *)&optionOn, sizeof(optionOn));
    if (rslt < 0) goto fail;

	return 0;
fail:
	rslt = -errno;
	close(bsdsock);
	return rslt;
}

void
neti_udp_close(struct netsocket_s *rlpsock)
{
	close(rlpsock->nativesock);
}

int neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, bool add)
{
	struct ip_mreqn multireq;
	int rslt;

	if (!is_multicast(localGroup)) return 0;

	multireq.imr_address.s_addr = INADDR_ANY;
	multireq.imr_ifindex = 0;
	multireq.imr_multiaddr.s_addr = localGroup;

	rslt = setsockopt(rlpsock->nativesock, IPPROTO_IP, (add) ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, (void *)&multireq, sizeof(multireq));
	return rslt;
}

int
neti_send_to(
	struct netsocket_s *netsock,
	struct netaddr_s *destaddr,
	uint8_t *data,
	size_t datalen
)
{
	struct sockaddr_in dadr;

	dadr.sin_family = AF_INET;
	dadr.sin_port = destaddr->port;
	dadr.sin_addr.s_addr = destaddr->addr;
	return sendto(netsock->nativesock, data, datalen, 0, (const struct sockaddr *)&dadr, sizeof(dadr));
}

//static
int
netiGetPkt(void)
{
	int rslt;
	struct netsocket_s *rlpsock;
	uint8_t *buf;
//	struct pktAddr_s
	netiHost_t remhost;
	uint8_t pktinfo[64];
	struct iovec bufvec[1];
	struct msghdr hdr;
	struct cmsghdr *cmp;
	ip4addr_t destaddr;

	buf = neti_newPacket(INPACKETSIZE);
	bufvec->iov_base = buf;
	bufvec->iov_len = INPACKETSIZE;

	/* need to do select on sockets here */
	/* and work out which socket has data */
	/* arrive with rlpsock pointing to the socket */
	rlpsock = NULL;

	hdr.msg_name = &remhost;
	hdr.msg_namelen = sizeof(struct sockaddr_in);
	hdr.msg_iov = bufvec;
	hdr.msg_iovlen = 1;
	hdr.msg_control = pktinfo;
	hdr.msg_controllen = sizeof(pktinfo);
	hdr.msg_flags = 0;

	destaddr = NETI_INADDR_ANY;

    rslt = recvmsg(rlpsock->nativesock, &hdr, 0);
	if (rslt < 0)
	{
		perror("recvmsg");
		return -1;
	}

	for (cmp = CMSG_FIRSTHDR(&hdr); cmp != NULL; cmp = CMSG_NXTHDR(&hdr, cmp))
	{
		if (cmp->cmsg_level == IPPROTO_IP && cmp->cmsg_type == IP_PKTINFO)
		{
			ip4addr_t pktaddr;

			if ( is_multicast( pktaddr = ((struct in_pktinfo *)(CMSG_DATA(cmp)))->ipi_addr.s_addr))
				destaddr = pktaddr;
		}
	}
	rlpProcessPacket(rlpsock, buf, rslt, destaddr, &remhost);
	return 0;
}

#elif CONFIG_STACK_WATERLOO	/* #if CONFIG_STACK_BSD */

void neti_init(void)
{
}

int neti_udp_open(struct rlpsocket_s *rlpsock, port_t localport)
{
	if (udp_open(&rlpsock->nativesock, localport, -1, 0, &netihandler) == 0)
		return -1;
	rlpsock->nativesock.usr_name = char *rlpsock;	/* use usr_name field to store reference pointer **EXPERIMENTAL** */
	return 0;

}

void
neti_udp_close(struct netsocket_s *rlpsock)
{
	udp_close(&rlpsock->nativesock);
}

int
neti_joinGroup(struct netsocket_s *rlpsock, ip4addr_t localGroup)
{
	int rslt;

	if (!isMulticast(localGroup)) return 0;
	rslt = mcast_join(localGroup);
	return -rslt;
}

int
neti_leaveGroup(struct netsocket_s *rlpsock, ip4addr_t localGroup)
{
	int rslt;

	if (!isMulticast(localGroup)) return 0;
	rslt = mcast_leave(localGroup);
	return -rslt;
}

int
neti_send_to(
	struct netsocket_s *netsock,
	struct netaddr_s *destaddr,
	uint8_t *data,
	size_t datalen
)
{
	sock_sendto((void*)&(netsock->nativesock), data, datalen, destaddr->addr, destaddr->port);
}

static int
netihandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr)
{
	struct netsocket_s *rlpsock;
	port_t srcPort;
	netiHost_t remhost;
	ip4addr_t destaddr;

	rlpsock = s->usr_name;
	if (rlpsock != NULL)
	{
/* warning Waterloo stack has not checked multicast destination yet */
		destaddr = ((in_Header *)hdr)->destination;
		remhost.addr = s->hisaddr;
		remhost.port = s->hisport;
		rlpProcessPacket(rlpsock, buf, dataLen, destaddr, &remhost);
	}
	return dataLen;
}

#endif	/* #elif CONFIG_STACK_BSD */

#endif	/* #if CONFIG_NET_IPV4 */
