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
/*
#tabs=4
*/
/*--------------------------------------------------------------------*/

#if CONFIG_STACK_LWIP && !defined(__netx_lwip_h__)
#define __netx_lwip_h__ 1

#include "lwip/udp.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/igmp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "netif/etharp.h"

#define acn_port_protect() sys_arch_protect()
#define acn_port_unprotect(pval) sys_arch_unprotect(pval)

#if CONFIG_NET_IPV4
#ifndef HAVE_port_t
  typedef uint16_t port_t;  /* net endpoint is a port */
  #define HAVE_port_t
#endif

#ifndef HAVE_ip4addr_t
  typedef uint32_t ip4addr_t; /* net group is a multicast address */
  #define HAVE_ip4addr_t
#endif

#ifndef HAVE_groupaddr_t
  typedef ip4addr_t groupaddr_t;
  #define HAVE_groupaddr_t
#endif

#define is_multicast(addr) (((addr) & htonl(0xf0000000)) == htonl(0xe0000000))

#define IP4_ADDR(ip4addr, a,b,c,d) \
        *ip4addr = htonl(((uint32_t)((a) & 0xff) << 24) | \
                               ((uint32_t)((b) & 0xff) << 16) | \
                               ((uint32_t)((c) & 0xff) << 8) | \
                                (uint32_t)((d) & 0xff))

#endif	/* CONFIG_NET_IPV4 */

typedef void netx_callback_t (
  /* component_event_t state, */
  void *param1,  /* does not seem to be used but might hold the addr of the callback routine */
  void *param2
);

/************************************************************************/
/*
  We use each stack's native structure for holding transport layer (UDP) addresses.
  This information is typedef'd to netx_addr_t.
  To unify the API we hide each stacks implementation of netx_addr_t behind macros
  which must be defined for each stack type.

  #define netx_PORT(netaddr_s_ptr)
  #define netx_INADDR(netaddr_s_ptr)
  #define netx_INIT_ADDR(addrp, inaddr, port)
  #define netx_INIT_ADDR_STATIC(inaddr, port)

  netx_PORT and netx_INADDR access the port and host address parts
  respectively. These should be lvalues so they can be assigned as well
  as read.

  netx_INIT_ADDR sets both address and port and initializes all fields
  netx_INIT_ADDR_STATIC expands to a static structure initializer

  Addresses are in "NETWORK order" (Big-Endian)

*/

/************************************************************************/

#if CONFIG_NET_IPV4
#define netx_FAMILY AF_INET
#endif /* CONFIG_NET_IPV4 */

typedef struct udp_pcb *netx_nativeSocket_t;
typedef struct sockaddr_in netx_addr_t;
/*
struct sockaddr_in {
  u8_t sin_len;
  u8_t sin_family;
  u16_t sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};
*/
/* operations performed on netx_addr_t */
#define netx_PORT(addrp) (addrp)->sin_port
#define netx_INADDR(addrp) (addrp)->sin_addr.s_addr
#define netx_INIT_ADDR_STATIC(inaddr, port) {sizeof(struct sockaddr_in), netx_FAMILY, (port), {inaddr}}
#define netx_INIT_ADDR(addrp, inaddr, port) ( \
		(addrp)->sin_len = sizeof(struct sockaddr_in), \
		(addrp)->sin_family = netx_FAMILY, \
		netx_INADDR(addrp) = (inaddr), \
		netx_PORT(addrp) = (port) \
	)

/************************************************************************/

typedef struct netxsocket_s netxsocket_t;

/* FIXME is netx_process_packet_t used */
typedef void netx_process_packet_t (
    netxsocket_t        *socket,
    const uint8_t       *data,
    int                  length,
    netx_addr_t         *dest,
    netx_addr_t         *source,
    void                *ref
);

/************************************************************************/
#if CONFIG_LOCALIP_ANY
struct netsocket_s {
	netx_nativeSocket_t nativesock;
	port_t localaddr;
};

/* operations when looking at netxsock_t */
#define NSK_PORT(x) ((x).localaddr)
#define NSK_INADDR(x) netx_INADDR_ANY

#ifndef HAVE_localaddr_t
	typedef port_t          localaddr_t;
	#define HAVE_localaddr_t
#endif

/* operation when looking at localaddr_t */
#define LCLAD_PORT(x) x
#define LCLAD_INADDR(x) netx_INADDR_ANY
#define netx_LCLADDR(x) netx_PORT(x)

#else /* !CONFIG_LOCALIP_ANY */

struct netsocket_s {
	netx_nativeSocket_t nativesock;
	netx_addr_t localaddr;
};
#define NSK_PORT(x) netx_PORT(&(x).localaddr)
#define NSK_INADDR(x) netx_INADDR(&(x).localaddr)

typedef netx_addr_t *localaddr_t;

#define LCLAD_PORT(x) netx_PORT(x)
#define LCLAD_INADDR(x) netx_INADDR(x)
#define netx_LCLADDR(x) (x)

#endif /* !CONFIG_LOCALIP_ANY */

/************************************************************************/
/* function prototypes for netxface.c */
void netx_handler(char *data, int length, netx_addr_t *source, netx_addr_t *dest);

extern void  netx_init(void);
extern int   netx_poll(void);
extern int   netx_udp_open(netxsocket_t *netsock, localaddr_t *localaddr);
extern void  netx_udp_close(netxsocket_t *netsock);
extern int   netx_send_to(netxsocket_t *netsock, const netx_addr_t *destaddr, void  *pkt, size_t datalen);
extern int   netx_change_group(netxsocket_t *netsock, ip4addr_t local_group, int operation);
extern void *netx_new_txbuf(int size);
extern void  netx_release_txbuf(void * pkt);
extern void  netx_free_txbuf(void *pkt);
extern char *netx_txbuf_data(void *pkt);

/* operation argument for netx_change_group */
#define netx_JOINGROUP IP_ADD_MEMBERSHIP
#define netx_LEAVEGROUP IP_DROP_MEMBERSHIP

#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr);
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr);
#endif /* CONFIG_NET_IPV4 */

#ifndef netx_PORT_NONE
#define netx_PORT_NONE 0
#endif

#ifndef netx_PORT_EPHEM
#define netx_PORT_EPHEM (port_t)0
#endif

#ifndef netx_INADDR_ANY
#define netx_INADDR_ANY ((ip4addr_t)0)
#endif

#ifndef netx_INADDR_NONE
#define netx_INADDR_NONE ((ip4addr_t)0xffffffff)
#endif

#ifndef netx_GROUP_UNICAST
#define netx_GROUP_UNICAST netx_INADDR_ANY
#endif

#ifndef netx_INIT_ADDR 
#define netx_INIT_ADDR(addrp, addr, port) (netx_INADDR(addrp) = (addr), netx_PORT(addrp) = (port))
#endif


#endif	/* #if CONFIG_STACK_LWIP && !defined(__netx_lwip_h__) */
