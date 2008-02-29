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

#ifndef __netiface_h__
#define __netiface_h__ 1

#include <string.h>
#include "opt.h"
#include "acn_arch.h"

#if CONFIG_EPI20
#include "epi20.h"
#endif

#if CONFIG_NET_IPV4
typedef uint16_t port_t;	/* net endpoint is a port */
typedef uint32_t ip4addr_t;	/* net group is a multicast address */
typedef ip4addr_t groupaddr_t;
#define is_multicast(addr) (((addr) & htonl(0xf0000000)) == htonl(0xe0000000))

/*
  We use the native structure for holding transport layer (UDP) addresses defined
  to neti_addr_t
  To unify the API we hide this behind macros which must be defined for each stack

  #define NETI_PORT(netaddr_s_ptr)
  #define NETI_INADDR(netaddr_s_ptr)
  #define NETI_INIT_ADDR(addrp, inaddr, port)
  #define NETI_INIT_ADDR_STATIC(inaddr, port)

  NETI_PORT and NETI_INADDR access the port and host address parts
  respectively. These should be lvalues so they can be assigned as well
  as read.

  NETI_INIT_ADDR sets both address and port and initializes all fields
  NETI_INIT_ADDR_STATIC expands to a static structure initializer
  
*/

#endif	/* CONFIG_NET_IPV4 */

#if CONFIG_STACK_LWIP
#include "lwip/udp.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/igmp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "netif/etharp.h"

#if CONFIG_NET_IPV4
#define NETI_FAMILY AF_INET
#endif

typedef struct udp_pcb *neti_nativeSocket_t;
typedef struct sockaddr_in neti_addr_t;
/*
struct sockaddr_in {
  u8_t sin_len;
  u8_t sin_family;
  u16_t sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};
*/
#define NETI_PORT(addrp) (addrp)->sin_port
#define NETI_INADDR(addrp) (addrp)->sin_addr.s_addr
#define NETI_INIT_ADDR_STATIC(inaddr, port) {sizeof(struct sockaddr_in), NETI_FAMILY, (port), {inaddr}}
#define NETI_INIT_ADDR(addrp, inaddr, port) ( \
		(addrp)->sin_len = sizeof(struct sockaddr_in), \
		(addrp)->sin_family = NETI_FAMILY, \
		NETI_INADDR(addrp) = (inaddr), \
		NETI_PORT(addrp) = (port) \
	)
#endif  /* of CONFIG_STACK_LWIP*/

#if CONFIG_STACK_BSD
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#if CONFIG_NET_IPV4
#define NETI_FAMILY AF_INET
#define NETI_INADDR_ANY INADDR_ANY
#endif

typedef int neti_nativeSocket_t;
typedef struct sockaddr_in neti_addr_t;

#define NETI_PORT(addrp) (addrp)->sin_port
#define NETI_INADDR(addrp) (addrp)->sin_addr.s_addr
#define NETI_INADDR_S(addrp) (addrp)->sin_addr
#define NETI_INIT_ADDR_STATIC(inaddr, port) {NETI_FAMILY, (port), {inaddr}}
#define NETI_INIT_ADDR(addrp, addr, port) ( \
		(addrp)->sin_family = NETI_FAMILY, \
		NETI_INADDR(addrp) = (addr), \
		NETI_PORT(addrp) = (port) \
	)


#endif /* CONFIG_STACK_BSD */

#if CONFIG_STACK_PATHWAY

#include "wattcp.h"

typedef udp_Socket neti_nativeSocket_t;
struct netiHost_s;
typedef struct netiHost_s neti_addr_t;

/* mainain in network byte order */
struct PACKED netiHost_s {
	port_t port;
	ip4addr_t addr;
};

#define NETI_PORT(addrp) (addrp)->port
#define NETI_INADDR(addrp) (addrp)->addr
#define NETI_DECLARE_ADDR(addr, inaddr, port) neti_addr_t addr = {(port), (inaddr)}
#endif /* CONFIG_STACK_PATHWAY */

#if CONFIG_LOCALIP_ANY
struct netsocket_s {
	neti_nativeSocket_t nativesock;
	port_t localaddr;
};
#define NSK_PORT(x) ((x).localaddr)
#define NSK_INADDR(x) NETI_INADDR_ANY

typedef port_t localaddr_t;
#define LCLAD_PORT(x) x
#define LCLAD_INADDR(x) NETI_INADDR_ANY
#define NETI_LCLADDR(x) NETI_PORT(x)

#else /* !CONFIG_LOCALIP_ANY */

struct netsocket_s {
	neti_nativeSocket_t nativesock;
	neti_addr_t localaddr;
};
#define NSK_PORT(x) NETI_PORT(&(x).localaddr)
#define NSK_INADDR(x) NETI_INADDR(&(x).localaddr)

typedef neti_addr_t *localaddr_t;
#define LCLAD_PORT(x) NETI_PORT(x)
#define LCLAD_INADDR(x) NETI_INADDR(x)
#define NETI_LCLADDR(x) (x)

#endif /* !CONFIG_LOCALIP_ANY */

extern void neti_init(void);
extern int neti_poll(struct netsocket_s **sockps, int numsocks);
extern int neti_udp_open(struct netsocket_s *netsock, localaddr_t localaddr);
extern void neti_udp_close(struct netsocket_s *netsock);

#if CONFIG_STACK_BSD
#define neti_udp_close(netsock) close((netsock)->nativesock)
#endif

#if CONFIG_STACK_PATHWAY
#define neti_udp_close(netsock) udp_close(&(netsock)->nativesock)
#endif

extern int neti_change_group(struct netsocket_s *netsock, ip4addr_t localGroup, int operation);
/* operation argument */
#if CONFIG_STACK_LWIP
#define NETI_JOINGROUP IP_ADD_MEMBERSHIP
#define NETI_LEAVEGROUP IP_DROP_MEMBERSHIP
#endif

#if CONFIG_STACK_BSD
#define NETI_JOINGROUP IP_ADD_MEMBERSHIP
#define NETI_LEAVEGROUP IP_DROP_MEMBERSHIP
#endif /* CONFIG_STACK_BSD */

#if CONFIG_STACK_PATHWAY
#define NETI_JOINGROUP 1
#define NETI_LEAVEGROUP 0
#endif /* CONFIG_STACK_PATHWAY */

extern int neti_send_to(struct netsocket_s *netsock, const neti_addr_t *destaddr, uint8_t *data, size_t datalen);

#if CONFIG_NET_IPV4
ip4addr_t neti_getmyip(neti_addr_t *destaddr);
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
extern __inline__ void transportAddrToHost(const uint8_t *transaddr, neti_addr_t *hostaddr);
extern __inline__ void transportAddrToHost(const uint8_t *transaddr, neti_addr_t *hostaddr)
{
#if CONFIG_STACK_LWIP
  UNUSED_ARG(transaddr);
  UNUSED_ARG(hostaddr);

	//hostaddr->sin_family = AF_INET;
	//memcpy(&hostaddr->sin_port, transaddr+1, 2);
	//memcpy(&hostaddr->sin_addr, transaddr+3, 4);
#endif /* CONFIG_STACK_LWIP */

#if CONFIG_STACK_BSD
	hostaddr->sin_family = AF_INET;
	memcpy(&hostaddr->sin_port, transaddr+1, 2);
	memcpy(&hostaddr->sin_addr, transaddr+3, 4);
#endif /* CONFIG_STACK_BSD */

#if CONFIG_STACK_PATHWAY
	memcpy(&hostaddr, transaddr+1, 6);
#endif	/* CONFIG_STACK_PATHWAY */
}

// TODO wrf These are not used, delete?
/* Both native and ACN formats are network byte order */
extern __inline__ void hostToTransportAddr(const neti_addr_t *hostaddr, uint8_t *transaddr);
extern __inline__ void hostToTransportAddr(const neti_addr_t *hostaddr, uint8_t *transaddr)
{
  //TODO: this make netiface dependent on SDT...
	//*transaddr = SDT_ADDR_IPV4;

#if CONFIG_STACK_LWIP
  UNUSED_ARG(hostaddr);
  UNUSED_ARG(transaddr);
	//memcpy(transaddr+1, &hostaddr->sin_port, 2);
	//memcpy(transaddr+3, &hostaddr->sin_addr, 4);
#endif /* CONFIG_STACK_LWIP */

#if CONFIG_STACK_BSD
	memcpy(transaddr+1, &hostaddr->sin_port, 2);
	memcpy(transaddr+3, &hostaddr->sin_addr, 4);
#endif /* CONFIG_STACK_BSD */

#if CONFIG_STACK_PATHWAY
	memcpy(transaddr+1, &hostaddr, 6);
#endif	/* CONFIG_STACK_PATHWAY */
}

#endif	/* CONFIG_SDT */

#ifndef NETI_PORT_NONE
#define NETI_PORT_NONE 0
#endif
#ifndef NETI_PORT_EPHEM
#define NETI_PORT_EPHEM (port_t)0
#endif
#ifndef NETI_INADDR_ANY
#define NETI_INADDR_ANY ((ip4addr_t)0)
#endif
#ifndef NETI_GROUP_UNICAST
#define NETI_GROUP_UNICAST NETI_INADDR_ANY
#endif
#ifndef NETI_INIT_ADDR 
#define NETI_INIT_ADDR(addrp, addr, port) (NETI_INADDR(addrp) = (addr), NETI_PORT(addrp) = (port))
#endif


#endif	/* #ifndef __netiface_h__ */
