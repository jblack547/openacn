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

#ifndef __netiface_h__
#define __netiface_h__ 1

#include "types.h"
#include <string.h>

#if CONFIG_EPI20
#include "epi20.h"
#endif

#if CONFIG_NET_IPV4

typedef uint16_t port_t;	/* net endpoint is a port */
typedef uint32_t ip4addr_t;	/* net group is a multicast address */
typedef ip4addr_t groupaddr_t;
/*
  netaddr_s represents a UDP level address
*/
struct netaddr_s {
	port_t port;
	ip4addr_t addr;
};

#define NETI_PORT_NONE 0
#define NETI_INADDR_ANY ((ip4addr_t)0)
#define NETI_GROUP_UNICAST ((ip4addr_t)0)

#define is_multicast(addr) (((addr) & 0xf0000000) == 0xe0000000)

#endif	/* CONFIG_NET_IPV4 */

#if CONFIG_STACK_BSD

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

typedef int neti_nativeSocket_t;
typedef struct sockaddr_in netiHost_t;

#if CONFIG_NET_IPV4
#define NETI_FAMILY AF_INET
#elif CONFIG_NET_IPV6
#define NETI_FAMILY AF_INET6
#endif

#elif CONFIG_STACK_PATHWAY

#include "wattcp.h"

typedef udp_Socket neti_nativeSocket_t;
struct netiHost_s;
typedef struct netiHost_s netiHost_t;

#endif /* CONFIG_STACK_PATHWAY */

/* mainain in network byte order */
struct PACKED netiHost_s {
	port_t port;
	ip4addr_t addr;
};

struct netsocket_s {
	neti_nativeSocket_t nativesock;
	port_t localport;
};

extern void neti_init(void);
extern int neti_poll(struct netsocket_s **sockps, int numsocks);
extern int neti_udp_open(struct netsocket_s *netsock, port_t localport);
extern void neti_udp_close(struct netsocket_s *netsock);

#if CONFIG_STACK_BSD
#define neti_udp_close(netsock) close((netsock)->nativesock)
#elif CONFIG_STACK_PATHWAY
#define neti_udp_close(netsock) udp_close(&(netsock)->nativesock)
#endif

extern int neti_change_group(struct netsocket_s *netsock, ip4addr_t localGroup, int operation);
/* operation argument */
#if CONFIG_STACK_BSD
#define NETI_JOINGROUP IP_ADD_MEMBERSHIP
#define NETI_LEAVEGROUP IP_DROP_MEMBERSHIP
#elif CONFIG_STACK_PATHWAY
#define NETI_JOINGROUP 1
#define NETI_LEAVEGROUP 0
#endif /* CONFIG_STACK_PATHWAY */

extern int neti_send_to(struct netsocket_s *netsock, struct netaddr_s *destaddr, uint8_t *data, size_t datalen);

#if CONFIG_NET_IPV4
ip4addr_t neti_getmyip(struct netaddr_s *destaddr);
#if CONFIG_STACK_PATHWAY
#define neti_getmyip(destaddr) (my_ip_addr)
#endif /* CONFIG_STACK_PATHWAY */
#endif /* CONFIG_NET_IPV4 */

#if CONFIG_SDT
/*
SDT packets use a standard transport address format for address and port which may differ from native format
*/
#include "acn_sdt.h"

/* Both native and ACN formats are network byte order */
extern __inline__ void transportAddrToHost(const uint8_t *transaddr, netiHost_t *hostaddr)
{
#if CONFIG_STACK_BSD
	hostaddr->sin_family = AF_INET;
	memcpy(&hostaddr->sin_port, transaddr+1, 2);
	memcpy(&hostaddr->sin_addr, transaddr+3, 4);
#elif CONFIG_STACK_PATHWAY
	memcpy(&hostaddr, transaddr+1, 6);
#endif	/* CONFIG_STACK_PATHWAY */
}

/* Both native and ACN formats are network byte order */
extern __inline__ void hostToTransportAddr(const netiHost_t *hostaddr, uint8_t *transaddr)
{
	*transaddr = SDT_ADDR_IPV4;
#if CONFIG_STACK_BSD
	memcpy(transaddr+1, &hostaddr->sin_port, 2);
	memcpy(transaddr+3, &hostaddr->sin_addr, 4);
#elif CONFIG_STACK_PATHWAY
	memcpy(transaddr+1, &hostaddr, 6);
#endif	/* CONFIG_STACK_PATHWAY */
}

#endif	/* CONFIG_SDT */

#endif	/* #ifndef __netiface_h__ */
