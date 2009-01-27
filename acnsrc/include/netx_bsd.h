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

#if (CONFIG_STACK_BSD || CONFIG_STACK_CYGWIN) && !defined(__netx_bsd_h__)
#define __netx_bsd_h__ 1

#include "acnip.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_EPI20
#include "epi20.h"

/* MAX_MTU is max size of Ethernet packet - see epi20 for discussion */
typedef char UDPPacket[MAX_MTU];
#endif  /* CONFIG_EPI20 */

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

typedef int netx_nativeSocket_t;
typedef struct sockaddr_in netx_addr_t;

/* operations performed on netx_addr_t */
#define netx_PORT(addrp) (addrp)->sin_port
#define netx_INADDR(addrp) (addrp)->sin_addr.s_addr
#define netx_INIT_ADDR_STATIC(inaddr, port) {netx_FAMILY, (port), {inaddr}}
#define netx_INIT_ADDR(addrp, addr, port) ( \
    (addrp)->sin_family = netx_FAMILY, \
    netx_INADDR(addrp) = (addr), \
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
struct netxsocket_s {
  netx_nativeSocket_t    nativesock;      /* pointer to native socket structure */
  port_t                 localaddr;       /* local address */
  netx_process_packet_t *data_callback;   /* pointer to call back when data is available */
};
#define NETX_SOCK_HAS_CALLBACK 1

/* operations when looking at netxsock_t */
#define NSK_PORT(x) ((x)->localaddr)
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

struct netxsocket_s {
  netx_nativeSocket_t   *nativesock;     /* pointer to native socket structure */
  netx_addr_t            localaddr;      /* local address */
  netx_process_packet_t *data_callback;  /* pointer to call back when data is available */
};
#define NETX_SOCK_HAS_CALLBACK 1

#define NSK_PORT(x) netx_PORT(&(x)->localaddr)
#define NSK_INADDR(x) netx_INADDR(&(x)->localaddr)

typedef netx_addr_t *localaddr_t;

#define LCLAD_PORT(x) netx_PORT(x)
#define LCLAD_INADDR(x) netx_INADDR(x)

#define netx_LCLADDR(x) (x)

#endif /* !CONFIG_LOCALIP_ANY */

/************************************************************************/
/* function prototypes for netxface.c */
/* TODO: since these functions are or should be common to all platforms, should they be moved to the netface.h file? */
void netx_handler(char *data, int length, netx_addr_t *source, netx_addr_t *dest);

extern void  netx_init(void);
extern int   netx_startup(void);
extern int   netx_shutdown(void);
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
/* #define netx_udp_close(netsock) close((netsock)->nativesock) */

#define netx_JOINGROUP IP_ADD_MEMBERSHIP
#define netx_LEAVEGROUP IP_DROP_MEMBERSHIP

/************************************************************************/
#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr);
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr);
#endif /* CONFIG_NET_IPV4 */

/************************************************************************/
#ifndef netx_PORT_NONE
#define netx_PORT_NONE 0
#endif

#ifndef netx_PORT_HOLD
#define netx_PORT_HOLD 65535
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

#ifndef netx_SOCK_NONE
#define netx_SOCK_NONE 0
#endif

#ifdef __cplusplus
}
#endif

#endif  /* #if (CONFIG_STACK_BSD || CONFIG_STACK_CYGWIN) && !defined(__netx_bsd_h__) */
