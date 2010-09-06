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
#tabs=3s
*/
/*--------------------------------------------------------------------*/

#ifndef __netxface_h__
#define __netxface_h__ 1

#include <string.h>
#include "opt.h"
#include "acnstdtypes.h"
#include "acnip.h"
#if CONFIG_EPI20
#include "epi20.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/*
EPI20 deifnes MTU
*/
#if CONFIG_EPI20
/* MAX_MTU is max size of Ethernet packet - see epi20 for discussion */
typedef uint8_t UDPPacket[MAX_MTU];
#endif

/************************************************************************/
/*
Need to establish netxsocket_t as structure here - we fill in the
structure later.
*/
typedef struct netxsocket_s netxsocket_t;    /* netxsocket_s defined below */

/************************************************************************/
/*
Netxface is not supposed to be a layer - simply some glue, however if it
is shared by both SLP and RLP it needs to store callback pointers.
Otherwise it can be hard coded.
*/
#if (CONFIG_SLP + CONFIG_RLP) > 1
#define NETX_SOCK_HAS_CALLBACK 1
#endif /* (CONFIG_SLP + CONFIG_RLP) > 1 */

/************************************************************************/
/*
Now include stack dependent stuff
*/

#if (CONFIG_STACK_BSD || CONFIG_STACK_CYGWIN)
#include "netx_bsd.h"
#elif CONFIG_STACK_LWIP
#include "netx_lwip.h"
#elif CONFIG_STACK_PATHWAY
#include "netx_pathway.h"
#elif CONFIG_STACK_NETBURNER
#include "netx_netburner.h"
#elif CONFIG_STACK_WIN32
#include "netx_win32.h"
#endif

/************************************************************************/

#if NETX_SOCK_HAS_CALLBACK
typedef void netx_process_packet_t(
   netxsocket_t   *socket,
   const uint8_t  *data,
   int             length,
   groupaddr_t     group,
   netx_addr_t    *source
);
#endif /* NETX_SOCK_HAS_CALLBACK */

/************************************************************************/
/*
Now we can fill in netxsocket_s
*/
struct netxsocket_s {
   netx_nativeSocket_t     nativesock;      /* pointer to native socket structure */
   localaddr_t             localaddr;       /* local address */
#if NETX_SOCK_HAS_CALLBACK
   netx_process_packet_t  *data_callback;   /* pointer to call back when data is available */
#endif /* NETX_SOCK_HAS_CALLBACK */
};

/************************************************************************/
/*
function prototypes for netxface.c
*/
extern void  netx_init(void);
extern int   netx_startup(void);
extern int   netx_shutdown(void);
extern int   netx_poll(void);
extern int   netx_udp_open(netxsocket_t *netsock, localaddr_arg_t localaddr);
extern void  netx_udp_close(netxsocket_t *netsock);
extern int   netx_send_to(netxsocket_t *netsock, const netx_addr_t *destaddr, void  *pkt, ssize_t datalen);
extern int   netx_change_group(netxsocket_t *netsock, ip4addr_t local_group, int operation);
extern void *netx_new_txbuf(int size);
extern void  netx_release_txbuf(void * pkt);
extern void  netx_free_txbuf(void *pkt);
extern char *netx_txbuf_data(void *pkt);

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef __netxface_h__ */
