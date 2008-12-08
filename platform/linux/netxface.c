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
 * Neither the name of Engineering Arts nor the names of its
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

	$Id: netxface.c 109 2008-02-29 20:13:47Z bflorac $

*/
/*--------------------------------------------------------------------*/
#include "opt.h"
#include "acn_arch.h"
#include "netxface.h"
#include "rlp.h"
#include "rlpmem.h"
#include "acnlog.h"

#define INPACKETSIZE DEFAULT_MTU

/************************************************************************/
/*
  Initialize networking
*/
void netx_init(void)
{
	return;
}

/************************************************************************/
/*
  netx_udp_open()
    Open a "socket" with a given local port number
    The call returns 0 if OK, non-zero if it fails.
*/
static const int optionOn = 1;
static const int optionOff = 0;

int
netx_udp_open(struct netxsocket_s *rlpsock, localaddr_t *localaddr)
{
	int bsdsock;
	int rslt;
#if CONFIG_LOCALIP_ANY
	netx_addr_t bindaddr;
#endif
	netx_addr_t *bindp;

	bsdsock = socket(PF_INET, SOCK_DGRAM, 0);
	if (bsdsock < 0) return -errno;
/*
	rslt = fctl(bsdsock, F_SETFL, O_NONBLOCK);
	if (rslt < 0) goto fail;
*/
    rslt = setsockopt (bsdsock, SOL_SOCKET, SO_REUSEADDR, (void *)&optionOn, sizeof(optionOn));
	if (rslt < 0) goto fail;

#if CONFIG_LOCALIP_ANY
	bindp = &bindaddr;
	NETI_INIT_ADDR(bindp, LCLAD_INADDR(localaddr), LCLAD_PORT(localaddr));
#else
	bindp = *localaddr;
#endif

	/*
	FIXME
	If we simply use INADDR_ANY when binding, then sockets may receive spurious 
	multicast messages. These will be corrctly rejected but have already made it
	a long way up the stack.
	We should enumerate our interfaces and pick the default multicast interface.

	hint: "In determining or selecting outgoing interfaces, the following
	ioctls might be useful: SIOCGIFADDR (to get an interface's address),
	SIOCGIFCONF (to get the list of all the interfaces) and SIOCGIFFLAGS
	(to get an interface's flags and, thus, determine whether the
	interface is multicast capable or not -the IFF_MULTICAST flag-)."
	man netdevice for more info.
	*/

	rslt = bind(bsdsock, (struct sockaddr *)bindp, sizeof(netx_addr_t));
	if (rslt < 0) goto fail;

	rlpsock->nativesock = bsdsock;
	if (LCLAD_PORT(localaddr) == NETI_PORT_EPHEM)
	{
		/* find the ephemeral port which has been assigned */
		socklen_t addrlen;

		addrlen = sizeof(netx_addr_t);
		rslt = getsockname(bsdsock, (struct sockaddr *)bindp, &addrlen);
		if (rslt < 0) goto fail;
	}
#if CONFIG_LOCALIP_ANY
	NSK_PORT(*rlpsock) = NETI_PORT(bindp);
#else
	memcpy(&rlpsock->localaddr, bindp, sizeof(netx_addr_t));
#endif

	/* we will need information on destination address used */
	rslt = setsockopt (bsdsock, IPPROTO_IP, IP_PKTINFO, (void *)&optionOn, sizeof(optionOn));
	if (rslt < 0) goto fail;

	/* turn on loop-back of multicast packets */
	/*
	FIXME
	We should dynamically manage IP_MULTICAST_LOOP and only turn it on when
	we need to (i.e. when a client within the host wants to receive on a group
	which is also generated within the host).
	*/
	rslt = setsockopt (bsdsock, IPPROTO_IP, IP_MULTICAST_LOOP, (void *)&optionOn, sizeof(optionOn));
	if (rslt < 0) goto fail;

	return 0;
fail:
	rslt = -errno;
	close(bsdsock);
	return rslt;
}

int
netx_change_group(struct netxsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
	int rslt;
	struct ip_mreqn multireq;

	if (!is_multicast(localGroup)) return 0;

#if CONFIG_LOCALIP_ANY
	multireq.imr_address.s_addr = INADDR_ANY;
#else
	multireq.imr_address.s_addr = NSK_INADDR(*rlpsock);
#endif
	multireq.imr_ifindex = 0;
	multireq.imr_multiaddr.s_addr = localGroup;

	rslt = setsockopt(rlpsock->nativesock, IPPROTO_IP, operation, (void *)&multireq, sizeof(multireq));
	printf("setsockopt %s returns %d\n", 
		(operation == NETI_JOINGROUP) ? "join group" : (operation == NETI_LEAVEGROUP) ? "leave group" : "unknown",
		rslt);
/*
*/
	return rslt;
}

/************************************************************************/
/*
  netx_send_to()
    Send message to give address
    The call returns the number of characters sent, or negitive if an error occurred. 
*/

int
netx_send_to(
	struct netxsocket_s *netsock,
	const netx_addr_t *destaddr,
	uint8_t *data,
	size_t datalen
)
{
	return sendto(netsock->nativesock, data, datalen, 0, (const struct sockaddr *)destaddr, sizeof(netx_addr_t));
}
/************************************************************************/
/*
  Poll for input
*/
#include <poll.h>
#include <stdlib.h>

extern uint8_t *netx_newpacket(int size);
#define netx_newpacket(size) malloc(size)
extern void *netx_freepacket(uint8_t *pkt);
#define netx_freepacket(pkt) free(pkt)

int
netx_poll(struct netxsocket_s **sockps, int numsocks)
{
	struct pollfd pfd[MAX_RLP_SOCKETS];
	struct netxsocket_s *nsocks[MAX_RLP_SOCKETS];
	int i;
	int rslt;
	uint8_t *buf;
	int haderr;

	UNUSED_ARG(sockps);

	if (sockps == NULL)
	{
		struct netxsocket_s *sockp;

		for (sockp = rlpm_first_netsock(), i = 0; sockp != NULL; sockp = rlpm_next_netsock(sockp), ++i)
		{
			nsocks[i] = sockp;
			pfd[i].fd = sockp->nativesock;
			pfd[i].events = POLLIN;
		}
		numsocks = i;
		sockps = nsocks;
	}
	else
	{
		for (i = 0; i < numsocks; ++i)
		{
			pfd[i].fd = sockps[i]->nativesock;
			pfd[i].events = POLLIN;
		}
	}

	rslt = poll(pfd, numsocks, 0);
	if (rslt <= 0) return rslt;

	buf = netx_newpacket(INPACKETSIZE);
	haderr = 0;

	for (i = 0; i < numsocks; ++i)
		if ((pfd[i].revents & POLLIN))
		{
			uint8_t pktinfo[64];
			struct iovec bufvec[1];
			struct msghdr hdr;
			struct cmsghdr *cmp;
			ip4addr_t dest_inaddr;
			struct netxsocket_s *netsock;
			netx_addr_t remhost;

			netsock = sockps[i];
			bufvec->iov_base = buf;
			bufvec->iov_len = INPACKETSIZE;
			hdr.msg_name = &remhost;
			hdr.msg_namelen = sizeof(remhost);
			hdr.msg_iov = bufvec;
			hdr.msg_iovlen = 1;
			hdr.msg_control = pktinfo;
			hdr.msg_controllen = sizeof(pktinfo);
			hdr.msg_flags = 0;

			dest_inaddr = NETI_GROUP_UNICAST;

    		rslt = recvmsg(netsock->nativesock, &hdr, 0);
			if (rslt < 0)
			{
				haderr = errno;
				perror("recvmsg");
				continue;
			}

			for (cmp = CMSG_FIRSTHDR(&hdr); cmp != NULL; cmp = CMSG_NXTHDR(&hdr, cmp))
			{
				if (cmp->cmsg_level == IPPROTO_IP && cmp->cmsg_type == IP_PKTINFO)
				{
					ip4addr_t pktaddr;

					pktaddr = ((struct in_pktinfo *)(CMSG_DATA(cmp)))->ipi_addr.s_addr;
					if (is_multicast(pktaddr))
					{
						dest_inaddr = pktaddr;
					}
					printf("Socket %d: packet for %8x:%u\n", netsock->nativesock, ntohl(pktaddr), ntohs(NSK_PORT(*netsock)));
				}
			}
			rlp_process_packet(netsock, buf, rslt, dest_inaddr, &remhost);
		}

	netx_freepacket(buf);
	return haderr;
}

/************************************************************************/
/*
  netx_getmyip()

*/

#if CONFIG_NET_IPV4

ip4addr_t
netx_getmyip(netx_addr_t *destaddr)
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
	UNUSED_ARG(destaddr);

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
#endif	/* CONFIG_NET_IPV4 */
/************************************************************************/
