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
#include "rlpmem.h"

//extern uint8_t *neti_newPacket(int size);

#define INPACKETSIZE DEFAULT_MTU

/*--------------------------------------------------------------------*/
/*
  Initialize networking
*/

void neti_init(void)
{
	return;
}

/*--------------------------------------------------------------------*/
/*
  neti_udp_open

  Open a "socket" with a given local port number
*/

#if CONFIG_STACK_BSD

static const int optionOn = 1;
static const int optionOff = 0;

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
/*
	rslt = fctl(bsdsock, F_SETFL, O_NONBLOCK);
	if (rslt < 0) goto fail;
*/
    rslt = setsockopt (bsdsock, SOL_SOCKET, SO_REUSEADDR, (void *)&optionOn, sizeof(optionOn));
	if (rslt < 0) goto fail;

	localaddr.gen.sa_family = NETI_FAMILY;
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

#elif CONFIG_STACK_PATHWAY

int neti_udp_open(struct rlpsocket_s *rlpsock, port_t localport)
{
	if (udp_open(&rlpsock->nativesock, localport, -1, 0, &netihandler) == 0)
		return -1;
	rlpsock->nativesock.usr_name = char *rlpsock;	/* use usr_name field to store reference pointer **EXPERIMENTAL** */
	return 0;
}

#endif	/* CONFIG_STACK_PATHWAY */

/*--------------------------------------------------------------------*/
/*
  neti_change_group
  Parameter operation specifies NETI_JOINGROUP or NETI_LEAVEGROUP
*/

#if CONFIG_STACK_BSD

int
neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
	int rslt;
	struct ip_mreqn multireq;

	if (!is_multicast(localGroup)) return 0;

	multireq.imr_address.s_addr = INADDR_ANY;
	multireq.imr_ifindex = 0;
	multireq.imr_multiaddr.s_addr = localGroup;

	rslt = setsockopt(rlpsock->nativesock, IPPROTO_IP, operation, (void *)&multireq, sizeof(multireq));
	return rslt;
}

#elif CONFIG_STACK_PATHWAY
int
neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
	int rslt;

	if (!isMulticast(localGroup)) return 0;

	if (operation == NETI_JOINGROUP)
		rslt = mcast_join(localGroup);
	else
		rslt = mcast_leave(localGroup);
	return -rslt;
}
#endif	/* CONFIG_STACK_PATHWAY */

/*--------------------------------------------------------------------*/
/*
*/

#if CONFIG_STACK_BSD

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

#elif CONFIG_STACK_PATHWAY

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

#endif	/* CONFIG_STACK_PATHWAY */

/*--------------------------------------------------------------------*/
/*
*/

#if CONFIG_STACK_BSD
#include <poll.h>
#include <stdlib.h>

extern uint8_t *neti_newpacket(int size);
#define neti_newpacket(size) malloc(size)
extern void *neti_freepacket(uint8_t *pkt);
#define neti_freepacket(pkt) free(pkt)

int
neti_poll(struct netsocket_s **sockps, int numsocks)
{
	struct pollfd pfd[MAX_RLP_SOCKETS];
	int i;
	int rslt;
	uint8_t *buf;
	int haderr;

	if (numsocks > MAX_RLP_SOCKETS) return -1;

	for (i = 0; i < numsocks; ++i)
	{
		pfd[i].fd = sockps[i]->nativesock;
		pfd[i].events = POLLIN;
	}

	rslt = poll(pfd, numsocks, 0);
	if (rslt <= 0) return rslt;

	buf = neti_newpacket(INPACKETSIZE);
	haderr = 0;

	for (i = 0; i < numsocks; ++i)
		if ((pfd[i].revents & POLLIN))
		{
			netiHost_t remhost;
			uint8_t pktinfo[64];
			struct iovec bufvec[1];
			struct msghdr hdr;
			struct cmsghdr *cmp;
			ip4addr_t destaddr;
			struct netsocket_s *rlpsock;

			rlpsock = sockps[i];
			bufvec->iov_base = buf;
			bufvec->iov_len = INPACKETSIZE;
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
				haderr = errno;
				continue;
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
			//rlpProcessPacket(rlpsock, buf, rslt, destaddr, &remhost);
		}

	neti_freepacket(buf);
	return 0;
}

#elif CONFIG_STACK_PATHWAY

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

int
neti_poll(struct netsocket_s **sockps, int numsocks)
{
	for (i = 0; i < numsocks; ++i)
	{
		tcp_tick(&sockps[i]->nativesock);
	}
}

#endif	/* CONFIG_STACK_PATHWAY */
/*--------------------------------------------------------------------*/
/*
*/
#if CONFIG_NET_IPV4

#if CONFIG_STACK_BSD

ip4addr_t
neti_getmyip(struct netaddr_s *destaddr)
{
/*
FIXME
Should find the local IP address which would be used to send to
the given remote address. For now we just get the first address
*/
	char scratchbuf[256];
	int rslt;
	struct addrinfo *ilist;
	ip4addr_t myaddr;

	static const struct addrinfo hint = {
		.ai_family = NETI_FAMILY,
		.ai_socktype = SOCK_DGRAM
	};

	rslt = gethostname(scratchbuf, sizeof(scratchbuf));
	if (rslt != 0) return 0;
	rslt = getaddrinfo(scratchbuf, NULL, &hint, &ilist);
	if (rslt != 0 || ilist == NULL) return 0;

	myaddr = ((struct sockaddr_in *)(ilist->ai_addr))->sin_addr.s_addr;

	freeaddrinfo(ilist);
	return myaddr;
}

#endif	/* CONFIG_STACK_BSD */
#endif	/* CONFIG_NET_IPV4 */

/*--------------------------------------------------------------------*/
/*
*/

/*--------------------------------------------------------------------*/
/*
*/

