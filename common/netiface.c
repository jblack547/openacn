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

	$Id$

*/
/*--------------------------------------------------------------------*/
#include "opt.h"
#include "acn_arch.h"
#include "netiface.h"
#include "rlp.h"
#include "rlpmem.h"
#include "acnlog.h"

#define INPACKETSIZE DEFAULT_MTU

/************************************************************************/
/* local memory */


/************************************************************************/
/* local prototypes */
#if CONFIG_STACK_LWIP
static void  netihandler(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port);
#endif

#if CONFIG_STACK_PATHWAY
static int netihandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr);
#endif

/************************************************************************/
/*
  Initialize networking
*/
void neti_init(void)
{
	return;
}

/************************************************************************/
/*
  neti_udp_open()
    Open a "socket" with a given local port number
    The call returns 0 if OK, non-zero if it fails.
*/
#if CONFIG_STACK_LWIP
int
neti_udp_open(struct netsocket_s *rlpsock, localaddr_t localaddr)
{
  struct udp_pcb *neti_pcb;        // common Protocol Control Block

  if (rlpsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETI, "neti_udp_open : already open");
    return -1;
  }

  neti_pcb = udp_new();
  if (!neti_pcb)
    return -1;

  rlpsock->nativesock = neti_pcb;

  // BIND:sets local_ip and local_port
  if (!udp_bind(neti_pcb, LCLAD_INADDR(localaddr), LCLAD_PORT(localaddr)) == ERR_OK)
    return -1;

  if (LCLAD_PORT(localaddr) == NETI_PORT_EPHEM) {
// if port was 0, then the stack should have given us a port, so assign it back
    NSK_PORT(*rlpsock) = neti_pcb->local_port;
  }  else {
    NSK_PORT(*rlpsock) = LCLAD_PORT(localaddr);
  }
#if !CONFIG_LOCALIP_ANY
	NSK_INADDR(*rlpsock) = LCLAD_INADDR(localaddr);
#endif

  // UDP Callback
  udp_recv(neti_pcb, netihandler, rlpsock);

  return 0;
}
#endif /* CONFIG_STACK_LWIP */

/************************************************************************/
#if CONFIG_STACK_BSD
static const int optionOn = 1;
static const int optionOff = 0;

int
neti_udp_open(struct netsocket_s *rlpsock, localaddr_t localaddr)
{
	int bsdsock;
	int rslt;
#if CONFIG_LOCALIP_ANY
	neti_addr_t bindaddr;
#endif
	neti_addr_t *bindp;

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
	bindp = localaddr;
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

	rslt = bind(bsdsock, (struct sockaddr *)bindp, sizeof(neti_addr_t));
	if (rslt < 0) goto fail;

	rlpsock->nativesock = bsdsock;
	if (LCLAD_PORT(localaddr) == NETI_PORT_EPHEM)
	{
		/* find the ephemeral port which has been assigned */
		socklen_t addrlen;

		addrlen = sizeof(neti_addr_t);
		rslt = getsockname(bsdsock, (struct sockaddr *)bindp, &addrlen);
		if (rslt < 0) goto fail;
	}
#if CONFIG_LOCALIP_ANY
	NSK_PORT(*rlpsock) = NETI_PORT(bindp);
#else
	memcpy(&rlpsock->localaddr, bindp, sizeof(neti_addr_t));
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
#endif /* CONFIG_STACK_BSD */

/************************************************************************/
#if CONFIG_STACK_PATHWAY
int 
neti_udp_open(struct netsocket_s *rlpsock, struct localaddr_s *localaddr)
{
	if (udp_open(&rlpsock->nativesock, LCLAD_PORT(localaddr), -1, 0, &netihandler) == 0)
		return -1;
	rlpsock->nativesock.usr_name = char *rlpsock;	/* use usr_name field to store reference pointer **EXPERIMENTAL** */
	return 0;
}
#endif	/* CONFIG_STACK_PATHWAY */


/************************************************************************/
/*
  neti_udp_close()
    Close "socket"
  
*/
#if CONFIG_STACK_LWIP
void 
neti_udp_close(struct netsocket_s *rlpsock)
{
  if (!rlpsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETI, "neti_udp_close : upd not open");
    return;
  }
  udp_remove((struct udp_pcb*) (rlpsock->nativesock));
  /* clear pointer */
  rlpsock->nativesock = NULL;
}
#endif /* CONFIG_STACK_LWIP */


/************************************************************************/
/*
  neti_change_group
    Parameter operation specifies NETI_JOINGROUP or NETI_LEAVEGROUP
    The call returns 0 if OK, non-zero if it fails.
*/

#if CONFIG_STACK_LWIP
int
neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
  struct ip_addr addr;

  UNUSED_ARG(rlpsock);

	if (!is_multicast(localGroup)) return -1;

  /* result = ERR_OK which is defined as zero so return value is consistent */
  addr.addr = localGroup;
  if (operation == NETI_JOINGROUP)
    return igmp_joingroup(&netif_default->ip_addr, &addr);
  else
    return igmp_leavegroup(&netif_default->ip_addr, &addr);
}
#endif

/************************************************************************/
#if CONFIG_STACK_BSD
int
neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, int operation)
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
#endif /* CONFIG_STACK_BSD */

/************************************************************************/
#if CONFIG_STACK_PATHWAY
int
neti_change_group(struct netsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
	int rslt;
  #error Return value here is not consistent with other stacks
	if (!isMulticast(localGroup)) return 0;

	if (operation == NETI_JOINGROUP)
		rslt = mcast_join(localGroup);
	else
		rslt = mcast_leave(localGroup);
	return -rslt;
}
#endif	/* CONFIG_STACK_PATHWAY */


/************************************************************************/
/*
  neti_send_to()
    Send message to give address
    The call returns the number of characters sent, or negitive if an error occurred. 
*/

#if CONFIG_STACK_LWIP
int
neti_send_to(
	struct netsocket_s *rlpsock,
	const neti_addr_t *destaddr,
	uint8_t *data,
	size_t datalen
)
{
  struct pbuf     *pkt;  /* Outgoing packet */
  struct eth_addr *ethaddr_ret;
  struct ip_addr  *ipaddr_ret;
  int              result;
  int              t_begin;
  struct ip_addr   dest_addr;
  ip4addr_t        addr;
  int              arp_index;

  if (!rlpsock || !rlpsock->nativesock)
    return -1;

  //struct ip_addr *ipaddr,
  addr = NETI_INADDR(destaddr);
  dest_addr.addr = addr;

  if (!is_multicast(addr)) {
    /* We have the arp queue turned off, so we need to make sure we have a ARP entry  */
    /* else it will fail on first try */
    result = -1;
    arp_index = etharp_find_addr(netif_default, &dest_addr, &ethaddr_ret, &ipaddr_ret);
    if (arp_index < 0) {
      result = etharp_query(netif_default, &dest_addr, NULL);
      if (result == ERR_OK) {
        result = -1;
        t_begin = sys_jiffies();
        /* wait 2 seconds (ARP TIMEOUT)*/
        while ((unsigned int)sys_jiffies() - t_begin < 2000) {
          arp_index = etharp_find_addr(netif_default, &dest_addr, &ethaddr_ret, &ipaddr_ret);
          if (arp_index >= 0) {
            result = 0;
            break;
          }
        }
      } else {
        acnlog(LOG_WARNING | LOG_NETI, "neti_send_to : ARP query failure");
        return -1;
      }
    } else
      result = 0;
  
    if (result != 0) {
      acnlog(LOG_WARNING | LOG_NETI, "neti_send_to : ARP failure: %d.%d.%d.%d", 
        ntohl(addr) >> 24 & 0x000000ff,
        ntohl(addr) >> 16 & 0x000000ff,
        ntohl(addr) >> 8 & 0x000000ff,
        ntohl(addr) & 0x000000ff);
      return result;
    }
  }

  pkt = pbuf_alloc(PBUF_TRANSPORT, datalen, PBUF_POOL);
  if(pkt) {
    memcpy(pkt->payload, data, datalen);
    result = udp_sendto((struct udp_pcb*) (rlpsock->nativesock), pkt, (struct ip_addr *)&NETI_INADDR(destaddr), NETI_PORT(destaddr));
    pbuf_free(pkt);
    /* 0 is OK */
    if (result == 0) {
      // we will assume it all went!
      return datalen;
    }
  } else {
    acnlog(LOG_WARNING | LOG_NETI, "neti_send_to : Unable to allocate pbuf");
  }
  /* didn't send */
  return -1;
}
#endif

/************************************************************************/
#if CONFIG_STACK_BSD
int
neti_send_to(
	struct netsocket_s *netsock,
	const neti_addr_t *destaddr,
	uint8_t *data,
	size_t datalen
)
{
	return sendto(netsock->nativesock, data, datalen, 0, (const struct sockaddr *)destaddr, sizeof(neti_addr_t));
}
#endif /* CONFIG_STACK_BSD */

/************************************************************************/
#if CONFIG_STACK_PATHWAY

int
neti_send_to(
	struct netsocket_s *netsock,
	neti_addr_t *destaddr,
	uint8_t *data,
	size_t datalen
)
{
	sock_sendto((void*)&(netsock->nativesock), data, datalen, NETI_INADDR(destaddr), NETI_PORT(destaddr));
}
#endif	/* CONFIG_STACK_PATHWAY */


/************************************************************************/
/*
  Poll for input
*/
#if CONFIG_STACK_LWIP
  /* function is not required for LWIP */
#endif

/************************************************************************/
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
	struct netsocket_s *nsocks[MAX_RLP_SOCKETS];
	int i;
	int rslt;
	uint8_t *buf;
	int haderr;

	UNUSED_ARG(sockps);

	if (sockps == NULL)
	{
		struct netsocket_s *sockp;

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

	buf = neti_newpacket(INPACKETSIZE);
	haderr = 0;

	for (i = 0; i < numsocks; ++i)
		if ((pfd[i].revents & POLLIN))
		{
			uint8_t pktinfo[64];
			struct iovec bufvec[1];
			struct msghdr hdr;
			struct cmsghdr *cmp;
			ip4addr_t dest_inaddr;
			struct netsocket_s *netsock;
			neti_addr_t remhost;

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
			rlp_process_packet(netsock, buf, rslt, dest_inaddr, &remhost, 0);
		}

	neti_freepacket(buf);
	return haderr;
}
#endif /* CONFIG_STACK_BSD */

/************************************************************************/
#if CONFIG_STACK_PATHWAY
int
neti_poll(struct netsocket_s **sockps, int numsocks)
{
	for (i = 0; i < numsocks; ++i)
	{
		tcp_tick(&sockps[i]->nativesock);
	}
}
#endif	/* CONFIG_STACK_PATHWAY */

/************************************************************************/
/* 
  netihandler()
    Socket call back 
*/
#if CONFIG_STACK_LWIP
/*
  This function is call for connections.  Our stack ensures us that if we get here, it is for us even 
  if multicast.  So to get here it either is Unicast or multicats with a matching port as 
  in this implementations we dont use ANY_PORT for local port
*/
static void 
netihandler(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port)
{
  neti_addr_t remhost;
  ip4addr_t dest_inaddr;

  // arg contains a netsock

  UNUSED_ARG(pcb);

  NETI_INADDR(&remhost) = addr->addr;
  NETI_PORT(&remhost) = port;

  // We don't have destination address in our callback
  // Turns out that the destinaion address is just after the location
  // of the source address
  dest_inaddr = ((struct ip_addr*)((char*)addr + sizeof(struct ip_addr)))->addr;

  /* pass pbuf up the chain so we can reference count usage */
  rlp_process_packet(arg, p->payload, p->tot_len, dest_inaddr, &remhost, p);
  pbuf_free(p);
}
#endif /* CONFIG_STACK_LWIP */

/************************************************************************/
#if CONFIG_STACK_PATHWAY
static int
netihandler(void *s, uint8 *data, int dataLen, tcp_PseudoHeader *pseudo, void *hdr)
{
	struct netsocket_s *rlpsock;
	port_t srcPort;
	neti_addr_t remhost;
	ip4addr_t dest_inaddr;

	rlpsock = s->usr_name;
	if (rlpsock != NULL)
	{
/* warning Waterloo stack has not checked multicast destination yet */
		dest_inaddr = ((in_Header *)hdr)->destination;
		NETI_INADDR(remhost) = s->hisaddr;
		NETI_PORT(remhost) = s->hisport;
		rlp_process_packet(rlpsock, buf, dataLen, dest_inaddr, &remhost, 0);
	}
	return dataLen;
}
#endif	/* CONFIG_STACK_PATHWAY */


/************************************************************************/
/*
  neti_getmyip()

*/

#if CONFIG_NET_IPV4

#if CONFIG_STACK_LWIP
ip4addr_t
neti_getmyip(neti_addr_t *destaddr)
{
  UNUSED_ARG(destaddr);
  return netif_default->ip_addr.addr;
}
#endif /* CONFIG_STACK_LWIP */

/************************************************************************/
#if CONFIG_STACK_BSD
ip4addr_t
neti_getmyip(neti_addr_t *destaddr)
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
#endif	/* CONFIG_STACK_BSD */

#endif	/* CONFIG_NET_IPV4 */
/************************************************************************/
