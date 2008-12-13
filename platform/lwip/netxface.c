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
#include "netxface.h"
#include "rlp.h"
#include "rlpmem.h"
#include "acnlog.h"

#define INPACKETSIZE DEFAULT_MTU

/************************************************************************/
/* local prototypes */
static void  netxhandler(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port);

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
int
netx_udp_open(struct netxsocket_s *rlpsock, localaddr_t localaddr)
{
  struct udp_pcb *netx_pcb;        /* common Protocol Control Block */

  if (rlpsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETI, "netx_udp_open : already open");
    return -1;
  }

  netx_pcb = udp_new();
  if (!netx_pcb)
    return -1;

  rlpsock->nativesock = netx_pcb;

  /* BIND:sets local_ip and local_port */
  if (!udp_bind(netx_pcb, LCLAD_INADDR(localaddr), LCLAD_PORT(localaddr)) == ERR_OK)
    return -1;

  if (LCLAD_PORT(localaddr) == NETI_PORT_EPHEM) {
/* if port was 0, then the stack should have given us a port, so assign it back */
    NSK_PORT(*rlpsock) = netx_pcb->local_port;
  }  else {
    NSK_PORT(*rlpsock) = LCLAD_PORT(localaddr);
  }
#if !CONFIG_LOCALIP_ANY
	NSK_INADDR(*rlpsock) = LCLAD_INADDR(localaddr);
#endif

  /* UDP Callback */
  udp_recv(netx_pcb, netxhandler, rlpsock);

  return 0;
}

/************************************************************************/
/*
  netx_udp_close()
    Close "socket"
  
*/
void 
netx_udp_close(struct netxsocket_s *rlpsock)
{
  if (!rlpsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETI, "netx_udp_close : upd not open");
    return;
  }
  udp_remove((struct udp_pcb*) (rlpsock->nativesock));
  /* clear pointer */
  rlpsock->nativesock = NULL;
}


/************************************************************************/
/*
  netx_change_group
    Parameter operation specifies NETI_JOINGROUP or NETI_LEAVEGROUP
    The call returns 0 if OK, non-zero if it fails.
*/

int
netx_change_group(struct netxsocket_s *rlpsock, ip4addr_t localGroup, int operation)
{
  struct ip_addr addr;

  UNUSED_ARG(rlpsock);

	if (!is_multicast(localGroup)) return -1;

  /* result = ERR_OK which is defined as zero so return value is consistent */
  addr.addr = localGroup;
  if (operation == NETI_JOINGROUP)
    return igmp_joingroup(&netxf_default->ip_addr, &addr);
  else
    return igmp_leavegroup(&netxf_default->ip_addr, &addr);
}

/************************************************************************/
/*
  netx_send_to()
    Send message to give address
    The call returns the number of characters sent, or negitive if an error occurred. 
*/

int
netx_send_to(
	struct netxsocket_s *rlpsock,
	const netx_addr_t *destaddr,
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

  /* struct ip_addr *ipaddr, */
  addr = NETI_INADDR(destaddr);
  dest_addr.addr = addr;

  if (!is_multicast(addr)) {
    /* We have the arp queue turned off, so we need to make sure we have a ARP entry  */
    /* else it will fail on first try */
    result = -1;
    arp_index = etharp_find_addr(netxf_default, &dest_addr, &ethaddr_ret, &ipaddr_ret);
    if (arp_index < 0) {
      result = etharp_query(netxf_default, &dest_addr, NULL);
      if (result == ERR_OK) {
        result = -1;
        t_begin = sys_jiffies();
        /* wait 2 seconds (ARP TIMEOUT)*/
        while ((unsigned int)sys_jiffies() - t_begin < 2000) {
          arp_index = etharp_find_addr(netxf_default, &dest_addr, &ethaddr_ret, &ipaddr_ret);
          if (arp_index >= 0) {
            result = 0;
            break;
          }
        }
      } else {
        acnlog(LOG_WARNING | LOG_NETI, "netx_send_to : ARP query failure");
        return -1;
      }
    } else
      result = 0;
  
    if (result != 0) {
      acnlog(LOG_WARNING | LOG_NETI, "netx_send_to : ARP failure: %d.%d.%d.%d", 
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
      /* we will assume it all went! */
      return datalen;
    }
  } else {
    acnlog(LOG_WARNING | LOG_NETI, "netx_send_to : Unable to allocate pbuf");
  }
  /* didn't send */
  return -1;
}

/************************************************************************/
/*
  Poll for input
*/
  /* function is not required for LWIP */


/************************************************************************/
/* 
  netxhandler()
    Socket call back 
*/
/*
  This function is call for connections.  Our stack ensures us that if we get here, it is for us even 
  if multicast.  So to get here it either is Unicast or multicats with a matching port as 
  in this implementations we dont use ANY_PORT for local port
*/
static void 
netxhandler(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, u16_t port)
{
  netx_addr_t remhost;
  ip4addr_t dest_inaddr;

  /* arg is contains netsock */

  UNUSED_ARG(pcb);

  NETI_INADDR(&remhost) = addr->addr;
  NETI_PORT(&remhost) = port;

  /* We don't have destination address in our callback */
  /* Turns out that the destinaion address is just after the location */
  /* of the source address */
  dest_inaddr = ((struct ip_addr*)((char*)addr + sizeof(struct ip_addr)))->addr;

  rlp_process_packet(arg, p->payload, p->tot_len, dest_inaddr, &remhost);
  pbuf_free(p);
}


/************************************************************************/
/*
  netx_getmyip()

*/

#if CONFIG_NET_IPV4

ip4addr_t
netx_getmyip(netx_addr_t *destaddr)
{
  UNUSED_ARG(destaddr);
  return netxf_default->ip_addr.addr;
}

#endif	/* CONFIG_NET_IPV4 */
/************************************************************************/
