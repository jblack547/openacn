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
#include "acn_arch.h"

#if CONFIG_STACK_NETBURNER

#include "constants.h"
#include "ip.h"
#include "udp.h"
#include "multicast.h"
#include "system.h"  // this is needed for ConfigRecord
#include "ip_addr.h" 

#include <startnet.h>
#include <netinterface.h>
#endif

#include "acn_arch.h"
#include "netxface.h"
#include "netsock.h"
#include "acnlog.h"
#include "inet.h"

/************************************************************************/
#define INPACKETSIZE DEFAULT_MTU
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_NETX, "%s :...", __func__)


/************************************************************************/
/* local memory */
OS_FIFO netx_fifo;    // FIFO to store all incoming UPD packets
int native_sock = 0;  // we dont really have socket but we need some marker...         


#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/*
  Initialize networking
*/
void netx_init(void)
{
  static bool initialized = 0;
  
  acnlog(LOG_DEBUG|LOG_NETX,"netx_init");
  
  if (!initialized) {
    /* init required sub modules */
    nsk_netsocks_init();
    OSFifoInit(&netx_fifo);
    initialized = 1;
  }
  return;
}


/************************************************************************/
void *netx_new_txbuf(int size)
{
  return new UDPPacket;
}

/************************************************************************/
void netx_free_txbuf(void * pkt)
{
  delete (UDPPacket*)pkt;
}

/************************************************************************/
/* Called when we are done with our buffer. This is done for ports that
 * require the buffer to be freed after a packet is sent. If the stack
 * take care of this itself, this can do nothing;
 */
void netx_release_txbuf(void * pkt)
{
//  delete (UDPPacket*)pkt;
}



/************************************************************************/
/* Get pointer to data inside tranmit buffer */
char *netx_txbuf_data(void *pkt)
{
  return (char*)(((UDPPacket *)pkt)->GetDataBuffer());
}


/************************************************************************/
/*
  netx_udp_open()
    Open a "socket" with a given local port number
    The call returns 0 if OK, non-zero if it fails.
*/
int netx_udp_open(netxsocket_t *netsock, localaddr_t *localaddr)
{
  /* open a unicast socket */ 
  LOG_FSTART();
    
  /* if this socket is already open */
  if (netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : already open");
    return -1;
  }
  
  /* flag that the socket is open */
  netsock->nativesock = &native_sock;

  /* save the passed in address/port number into the passed in netxsocket_s struct */
  NSK_PORT(netsock) = LCLAD_PORT(*localaddr);
  
  /* Register for rx UDP packets on this port and put them in this fifo. */
  /* The same fifo is used for each call to this routine. */
  RegisterUDPFifo(NSK_PORT(netsock), &netx_fifo);
  
  acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open : open port:%d", NSK_PORT(netsock));
  
  /* Note: A separate thread will call netx_poll() to look for received messages */
  return 0;
}


/************************************************************************/
/*
  netx_udp_close()
    Close "socket"
*/
void netx_udp_close(netxsocket_t *netsock)
{
  LOG_FSTART();

  /* if it's already closed */
  if (!netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_close : upd not open");
    return;
  }
  
  /* unregister fifo */
  UnregisterUDPFifo(NSK_PORT(netsock));

  /* clear flag that it's in use */ 
  netsock->nativesock = NULL;
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
  LOG_FSTART();

  /* if the IP passed in is not a valid multicast address */
  if (!is_multicast(local_group)) {
	 return -1;
  }
  
  acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group, port, %d, group: %s", NSK_PORT(netsock), inet_ntoa(local_group));

  /* result = ERR_OK which is defined as zero so return value is consistent */ 
  if (operation == netx_JOINGROUP) {
    /* Register to listen for UDP packets on the ACN port with this fifo */
    RegisterMulticastFifo(local_group, NSK_PORT(netsock), &netx_fifo);
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: added");
  } else {
    UnregisterMulticastFifo(local_group, NSK_PORT(netsock));
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: deleted");
  }
  return 0; /* OK */
}

/************************************************************************/
/*
  netx_send_to()
    Send message to given address
    The call returns the number of characters sent, or negitive if an error occurred. 
*/
int netx_send_to(
	netxsocket_t      *netsock,    // contains a flag if port is open and the local port number
	const netx_addr_t *destaddr,   // contians dest port and ip numbers
	void              *pkt,        // pointer data packet if type UPDPacket (return from netx_new_txbuf())
	size_t             datalen     // length of data
)
{
  ip4addr_t dest_addr;   // this is a long int
  int       dest_port;

  LOG_FSTART();

  /* if the port is not open or we have no packet */
  if (!netsock) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !netsocket");
    return -1;
  }
  if (!netsock->nativesock) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !nativesock");
    return -1;
  }
    
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: !pkt");
    return -1;
  }
  
  
  /* get dest IP and port from the calling routine */
  dest_addr = netx_INADDR(destaddr);
  dest_port = netx_PORT(destaddr);

  /* create a new UDP packet */
  /* Note: we don' need to copy as we are passing in a UDPPacket  
   *   get buffer
   * UDPPacket pkt;	            
   *   get address of where data will go in the packet
   * UdpBuffer = pkt.GetDataBuffer();
   *   copy data into packet
   * memcpy(UdpBuffer, data, datalen);
   */
  
  ((UDPPacket*)pkt)->SetSourcePort(NSK_PORT(netsock));      
  ((UDPPacket*)pkt)->SetDestinationPort(dest_port);
  ((UDPPacket*)pkt)->SetDataSize(datalen);         
  // send the packet to destination IP address
  ((UDPPacket*)pkt)->Send(dest_addr);	
  /* we will assume it all went! */
  /*acnlog(LOG_DEBUG | LOG_NETX , "netx_send_to: sent"); */
  return datalen;
}

/************************************************************************/
/*
  Poll for input
*/
int
netx_poll(void)
{
  WORD length;
  PBYTE pUDPData;
  netx_addr_t  source;
  netx_addr_t  dest;
  
  //LOG_FSTART();

  /* Construct a UDP packet object using the FIFO. */
  /* This constructor will block until we have received a packet in this fifo */
  /* However, we put a expire time on it so can bail out as we shut down. */
  UDPPacket newUDPPacket(&netx_fifo, TICKS_PER_SECOND);

  /* Did we get a valid packet? */
  /* If invalid, just ignore it and get the next one. */
  if (newUDPPacket.Validate()) { // UDPPacket member function
    acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: data");
    /* UDPPacket member functions to get variables */
    pUDPData = newUDPPacket.GetDataBuffer();         /* pointer to UDP data */
    length = newUDPPacket.GetDataSize();             /* length */
    netx_INADDR(&source) = newUDPPacket.GetSourceAddress();    /* IP source address */
    netx_INADDR(&dest) = newUDPPacket.GetDestinationAddress(); /* IP destination address */
    netx_PORT(&source) = newUDPPacket.GetSourcePort();             /* source port */
    netx_PORT(&dest) = newUDPPacket.GetDestinationPort();             /* source port */

    /* call our handler */
    //acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: source port: %d", newUDPPacket.GetSourcePort());
    acnlog(LOG_DEBUG | LOG_NETX,  "netx_poll, source port %d, address: %s", newUDPPacket.GetSourcePort(), inet_ntoa(newUDPPacket.GetSourceAddress()));
    acnlog(LOG_DEBUG | LOG_NETX,  "netx_poll, dest port %d, address: %s", newUDPPacket.GetDestinationPort(), inet_ntoa(newUDPPacket.GetDestinationAddress()));
    
        
    netx_handler(pUDPData, length, &source, &dest);
    acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: handled");
    
    /* send it to the ACN SLP UDP receiver */
    return 0;
  }
  /* Note: the UDP packet is automatically deleted with the call of the destructor here */
  //acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: timeout");
  return 0;
}      

/************************************************************************/
/* 
  netihandler()
    Socket call back 
    This is the routine that gets called when a new UDP message is available.
*/
void netx_handler(BYTE *data, int length, netx_addr_t *source, netx_addr_t *dest)
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
      (*socket->data_callback)(socket, data, length, dest, source, NULL);
      return;
    }
  }
  acnlog(LOG_DEBUG | LOG_NETX , "netx_handler: no callback, port: %d", LCLAD_PORT(host));
}


/************************************************************************/
/*
  netx_getmyip()

*/
#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr)
{
	int inf;  
  UNUSED_ARG(destaddr);
  
	// get interface
  inf = GetFirstInterface();
  
  // force refresh of structure
 	GetIfConfig(inf);		// perhaps this should be done globally once at boot!
 	
 	// get IP
  return InterfaceIP(inf);
}


/************************************************************************/
/*
  netx_getmyipmask()

*/
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr)
{
	int inf;  
  UNUSED_ARG(destaddr);
  
	// get interface
  inf = GetFirstInterface();
  
  // force refresh of structure
 	GetIfConfig(inf);		// perhaps this should be done globally once at boot!
 	
 	// get IP Mask
  return InterfaceMASK(inf);
}

#ifdef __cplusplus
}
#endif
#endif	/* CONFIG_NET_IPV4 */

