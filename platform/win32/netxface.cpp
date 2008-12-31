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
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"

#if CONFIG_STACK_WIN32
#include <malloc.h>
#include <ws2tcpip.h>

#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib") /* for getting ip mask */
/* #pragma comment(lib, "wsock32.lib") */
#pragma comment(lib, "ws2_32.lib") /* our winsock */

#include "netxface.h"
#include "netsock.h"
#include "ntoa.h"


/************************************************************************/
#define INPACKETSIZE DEFAULT_MTU
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_NETX, "%s :...", __func__)

/************************************************************************/
/* local memory */
WSADATA wsdata;


/************************************************************************/
/*
  Initialize networking
*/
void netx_init(void)
{
  static bool initialized_state = 0;
  

  if (initialized_state) {
    acnlog(LOG_INFO | LOG_NETX,"netx_init: already initialized");
    return;
  }
  /* don't process twice */
  initialized_state = 1;
  acnlog(LOG_DEBUG|LOG_NETX,"netx_init...");

  /* init required sub modules */
  nsk_netsocks_init();
  return;
}

/************************************************************************/
int netx_startup(void)
{
  int  res;

  /* init windows socket */
  res = WSAStartup(0x0202,&wsdata);
  if (res != 0) {
    acnlog(LOG_ERR | LOG_NETX, "netx_init : WSAStartup failed");
    return FAIL;
  }
  return OK;
  /* TODO: the above should verify the verions is correct */
}

/************************************************************************/
int netx_shutdown(void)
{
  /* Windows cleanup */
  WSACleanup();
  return OK;
}

/************************************************************************/
void *netx_new_txbuf(int size)
{
  UNUSED_ARG(size);
  return malloc(sizeof(UDPPacket));
}

/************************************************************************/
void netx_free_txbuf(void * pkt)
{
  free(pkt);
}

/************************************************************************/
/* Called when we are done with our buffer. This is done for ports that
 * require the buffer to be freed after a packet is sent. If the stack
 * takes care of this itself, this can do nothing;
 */
void netx_release_txbuf(void * pkt)
{
  UNUSED_ARG(pkt);
/*  delete (UDPPacket*)pkt; */
}



/************************************************************************/
/* Get pointer to data inside tranmit buffer */
char *netx_txbuf_data(void *pkt)
{
  return (char*)(pkt);
}


/************************************************************************/
/*
  netx_udp_open()
    Open a "socket" with a given local port number
    The call returns 0 if OK, non-zero if it fails.
*/

static const int optionOn = 1;
static const int optionOff = 0;

int netx_udp_open(netxsocket_t *netsock, localaddr_t *localaddr)
{
  netx_addr_t addr;
  int         ret;

  /* open a unicast socket */ 
  LOG_FSTART();

    
  /* if this socket is already open */
  if (netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : already open: %d", ntohs(LCLAD_PORT(*localaddr)));
    return FAIL;
  }
  
  /* flag that the socket is open */
  netsock->nativesock = socket(PF_INET, SOCK_DGRAM, 0);

  if (!netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : socket fail");
    return FAIL; /* FAIL */
  }

  #if 0
  Now done in netx_change_group()
  ret = setsockopt(netsock->nativesock, SOL_SOCKET, SO_REUSEADDR, (void *)&optionOn, sizeof(optionOn));
  if (ret < 0) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : setsockopt:SO_REUSEADDR fail");
    close(netsock->nativesock);
    netsock->nativesock = 0;
    return FAIL; /* FAIL */
  }
  #endif

  netx_INIT_ADDR(&addr, LCLAD_INADDR(*localaddr), LCLAD_PORT(*localaddr));

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

  ret =  bind(netsock->nativesock, (SOCKADDR *)&addr, sizeof(addr));

  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : bind fail, port:%d", ntohs(LCLAD_PORT(*localaddr)));
    closesocket(netsock->nativesock);
    netsock->nativesock = NULL;
    return FAIL; /* FAIL */
  }

  /* save the passed in address/port number into the passed in netxsocket_s struct */
  NSK_PORT(netsock) = LCLAD_PORT(*localaddr);

  /* this does not work in WIN32 */
  # if 0
  /* we will need information on destination address used */
	ret = setsockopt(netsock->nativesock, IPPROTO_IP, IP_PKTINFO, (void *)&optionOn, sizeof(optionOn));
	if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : setsockopt:IP_PKTINFO fail");
    close(netsock->nativesock);
    netsock->nativesock = 0;
    return FAIL; /* FAIL */
  }
  #endif
  
  acnlog(LOG_DEBUG | LOG_NETX, "netx_udp_open : port:%d", ntohs(NSK_PORT(netsock)));
  
  /* Note: A separate thread will call netx_poll() to look for received messages */
  return OK;
}


/************************************************************************/
/*
  netx_udp_close()
    Close "socket"
*/
void netx_udp_close(netxsocket_t *netsock)
{
  LOG_FSTART();
  netx_nativeSocket_t hold;

  /* if it's already closed */
  if (!netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_close : upd not open");
    return;
  }

  /* a little safer to mark it not used before we actually nuke it */
  hold = netsock->nativesock;
  /* clear flag that it's in use */ 
  netsock->nativesock = NULL;
  /* close socket */
  closesocket(hold);
}


/************************************************************************/
/*
  netx_change_group
    Parameter operation specifies netx_JOINGROUP or netx_LEAVEGROUP
    The call returns 0 if OK, non-zero if it fails.
    localGroup - multicast group IP address.
*/
int netx_change_group(netxsocket_t *netsock, ip4addr_t local_group, int operation)
{
  ip_mreq     mreq;
  int         ret;
  int         ip_ttl;

  LOG_FSTART();
  
  /* if the IP passed in is not a valid multicast address */
  if (!is_multicast(local_group)) {
	 return FAIL;
  }

  acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group, port: %d, group: %s", ntohs(NSK_PORT(netsock)), ntoa(local_group));

  /* socket should have some options if enabled for multicast */

  /* allow reuse of the local port number */
  ret = setsockopt(netsock->nativesock,  SOL_SOCKET, SO_REUSEADDR, (char *)&optionOn,  sizeof(optionOn));
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_change_group : setsockopt:SO_REUSEADDR fail");
    return FAIL; /* fail */
  }

  /* set the IP TTL */
  ip_ttl = 2;
  ret = setsockopt(netsock->nativesock,  IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ip_ttl,  sizeof(ip_ttl));
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_change_group : setsockopt:IP_MULTICAST_TTL fail");
    return FAIL; /* fail */
  }

  /* turn off loop back on multicast */
  ret = setsockopt(netsock->nativesock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&optionOff, sizeof(optionOff));
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_change_group : setsockopt:IP_MULTICAST_LOOP fail");
    return FAIL; /* fail */
  }

  mreq.imr_multiaddr.s_addr = local_group;
#if CONFIG_LOCALIP_ANY
	mreq.imr_interface.s_addr = INADDR_ANY;
#else
	mreq.imr_interface.s_addr = NSK_INADDR(*netsock);
#endif

  
  /* result = ERR_OK which is defined as zero so return value is consistent */ 
  if (operation == netx_JOINGROUP) {
    setsockopt(netsock->nativesock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: added");
  } else {
    setsockopt(netsock->nativesock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: dropped");
  }
  return OK; /* OK */
}

/************************************************************************/
/*
  netx_send_to()
    Send message to given address
    The call returns the number of characters sent, or negitive if an error occurred. 
*/
int netx_send_to(
	netxsocket_t      *netsock,    /* contains a flag if port is open and the local port number */
	const netx_addr_t *destaddr,   /* contians dest port and ip numbers */
	void              *pkt,        /* pointer data packet if type UPDPacket (return from netx_new_txbuf()) */
	size_t             datalen     /* length of data */
)
{
  netx_addr_t  dest_addr;

  LOG_FSTART();

  /* if the port is not open or we have no packet */
  if (!netsock) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !netsocket");
    return FAIL;
  }
  if (!netsock->nativesock) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !nativesock");
    return FAIL;
  }
    
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !pkt");
    return FAIL;
  }
  
  /* get dest IP and port from the calling routine */
  /* TODO: For now I'm going to copy to insure sin_family is set */
  netx_INIT_ADDR(&dest_addr, netx_INADDR(destaddr), netx_PORT(destaddr));

  /* create a new UDP packet */
  /* Note: we don' need to copy as we are passing in a UDPPacket  
   *   get buffer
   * UDPPacket pkt;	            
   *   get address of where data will go in the packet
   * UdpBuffer = pkt.GetDataBuffer();
   *   copy data into packet
   * memcpy(UdpBuffer, data, datalen);
   */
  
  sendto(netsock->nativesock, (char *)pkt, datalen, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr));

  /* we will assume it all went! */
  /*acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: sent"); */
  return datalen;
}

/************************************************************************/
/*
  Poll for input
*/
/* TODO: need to place this better */
/* TODO: This should not be hard code...and we should check size */
static UDPPacket recv_buffer;
int
netx_poll(void)
{
  int                 length;
  int                 addr_len = sizeof(netx_addr_t);
  fd_set              socks;
  netxsocket_t       *nsk; 
  netx_nativeSocket_t high_sock = 0;
  struct timeval      timeout;  /* Timeout for select */
  int                 readsocks;
 
  netx_addr_t         source;
  netx_addr_t         dest;
  
  /* LOG_FSTART(); */

  FD_ZERO(&socks);

  /* abort if we have no sockets yet */
  nsk = nsk_first_netsock();
  if (!nsk) {
    /* acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: no sockets"); */
    return FAIL;
  }
  while (nsk) {
    /* make sure we assignged the socket */
    if (nsk->nativesock) {
      FD_SET(nsk->nativesock,&socks);
      if (nsk->nativesock > high_sock) {
        high_sock = nsk->nativesock;
      }
    }
    nsk = nsk_next_netsock(nsk);
  }
  /* perhaps none were assigned */
  if (high_sock == 0) {
    return FAIL;
  }
  
  /* TODO: what should timeout be? */
  timeout.tv_sec = 10;
	timeout.tv_usec = 0;
  readsocks = select(high_sock+1, &socks, NULL, NULL, &timeout);
  if (readsocks < 0) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: select fail: %d", WSAGetLastError());
    return FAIL; /* fail */
  }
  if (readsocks > 0) {
    nsk = nsk_first_netsock();
    while (nsk && nsk->nativesock) {
      if (FD_ISSET(nsk->nativesock,&socks)) {
        /* acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: recvfrom..."); */
        length = recvfrom(nsk->nativesock, recv_buffer, sizeof(recv_buffer), 0, (SOCKADDR *)&source, &addr_len);
        if (length < 0) {
          acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: recvfrom fail: %d", WSAGetLastError());
          return FAIL; /* fail */
        }
        if (length > 0) {
          /* TODO: This need to be network in localaddress form! */
          netx_PORT(&dest) = LCLAD_PORT(nsk->localaddr);
          netx_INADDR(&dest) = LCLAD_INADDR(nsk->localaddr);

          /* acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: handoff"); */
          netx_handler(recv_buffer, length, &source, &dest);
          /* acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: handled"); */
/*          return OK; */
        } else {
          acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: length = 0");
          return OK; /* ok but no data */
        }
      }
      nsk = nsk_next_netsock(nsk);
    }
  } 
  /* acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: no data"); */
  return OK;
}      

/************************************************************************/
/* 
  netihandler()
    Socket call back 
    This is the routine that gets called when a new UDP message is available.
*/
void netx_handler(char *data, int length, netx_addr_t *source, netx_addr_t *dest)
{
  netxsocket_t *socket;
  localaddr_t   host;

  acnlog(LOG_DEBUG | LOG_NETX , "netx_handler: ...");

  /* save get destination address */
#if CONFIG_LOCALIP_ANY  
  LCLAD_PORT(host) = netx_PORT(dest);
#else
  LCLAD_INADDR(host) = netx_INADDR(dest);
  LCLAD_PORT(host) = netx_PORT(dest);
#endif  
  
  /* see if we have anyone registered for this socket */
  socket = nsk_find_netsock(&host);
  if (socket) {
    if (socket->data_callback) {
      (*socket->data_callback)(socket, (uint8_t *)data, length, dest, source, NULL);
      return;
    }
  }
  acnlog(LOG_DEBUG | LOG_NETX , "netx_handler: no callback, port: %d", ntohs(LCLAD_PORT(host)));
}


/************************************************************************/
/*
  netx_getmyip()
  Note: this only returns the fisrt one found and may not be correct if there are multple NICs
  or multiple IP address in one NIC
  This function return address in network format
*/
#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr)
{
/*
FIXME
Should find the local IP address which would be used to send to
the given remote address. For now we just get the first address
*/
ip4addr_t         result;
IP_ADAPTER_INFO * FixedInfo;
ULONG ulOutBufLen;

UNUSED_ARG(destaddr);

FixedInfo = (IP_ADAPTER_INFO *) malloc( sizeof( IP_ADAPTER_INFO ) );
ulOutBufLen = sizeof( IP_ADAPTER_INFO );

/* FIXME: this will fail if there is more than one adaptor or if adaptor has more than on IP */
/* if this fails, the ulOutBufLen will be filled with the size of the buffer needed...
   and then we can retry */
if ( ERROR_SUCCESS != GetAdaptersInfo( FixedInfo, &ulOutBufLen ) ) {
  return 0;
}

result = inet_addr( FixedInfo->IpAddressList.IpAddress.String );

free(FixedInfo);

return result;

#if 0
/* this works if we need to get MAC at some time! */
printf("your MAC is %02x-%02x-%02x-%02x-%02x-%02x\n", 
       FixedInfo->Address[0],
       FixedInfo->Address[1],
       FixedInfo->Address[2],
       FixedInfo->Address[3],
       FixedInfo->Address[4],
       FixedInfo->Address[5]
);
*/
#endif

/* this works too...
/* 
	char    s[256];
  struct  hostent *local_host;
  struct  in_addr *in;

  UNUSED_ARG(destaddr);

  gethostname(s, 256);
  local_host = gethostbyname(s);
  if (local_host) {
    in = (struct in_addr *) (local_host->h_addr_list[0]);
    return (in->s_addr);
  }
  return 0;
*/
}


/************************************************************************/
/*
  netx_getmyipmask()
  Note: this only returns the fisrt one found and may not be correct if there are multple NICs
  or multiple IP address in one NIC
  This function return mask in network format
*/
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr)
{
ip4addr_t         result;
IP_ADAPTER_INFO  *FixedInfo;
ULONG             ulOutBufLen;

UNUSED_ARG(destaddr);

FixedInfo = (IP_ADAPTER_INFO *) malloc( sizeof( IP_ADAPTER_INFO ) );
ulOutBufLen = sizeof( IP_ADAPTER_INFO );

/* FIXME: this will fail if there is more than one adaptor or if adaptor has more than on IP */
if ( ERROR_SUCCESS != GetAdaptersInfo( FixedInfo, &ulOutBufLen ) ) {
  return 0;
}
result = inet_addr( FixedInfo->IpAddressList.IpMask.String );

free(FixedInfo);

return result;
}

#endif	/* CONFIG_NET_IPV4 */

#endif /* CONFIG_STACK_WIN32 */

