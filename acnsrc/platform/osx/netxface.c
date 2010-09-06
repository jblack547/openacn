/************************************************************************/
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

  $Id: netxface.c 332 2010-09-02 16:53:11Z philipnye $

#tabs=2s
*/
/************************************************************************/
/*
Notes on use of interfaces, ports and multicast addressing (group
addressing).

This explanation is relevant to configuration options CONFIG_LOCALIP_ANY
and STACK_RETURNS_DEST_ADDR, The following applies to UDP on IPv4. UDP
on IPv6 will be similar, but transports other than UDP may be very
different.

IP addresses and interfaces:

A socket represents an endpoint and we bind it to an "interface" and
port. The interface is represented by  an IP address, but this can be
misleading because of the way multicast addressing works.

When a packet is received incoming packet, the stack identifies the
"interface" (and therefore the socket to receive the packet), by the
incoming destination address in the packet. However, when this address
is a multicast address, there is no interface information there.
Therefore, simply binding a socket to a specific unicast IP address is
not helpful.

The exception is the case where there are multiple physical interfaces 
on separate networks which may be within different multicast zones. In
this case, the stack uses rules based on its routing tables.

For this reason, it is usually best to open any socket intended to
receive multicast using IN_ADDR_ANY as the interface and rely on the
stack to worry about interfaces. So while this code allows for sockets
to be bound to specific interfaces (IP addresses), there is no guarantee
that doing so will prevent multicast traffic from other networks
arriving at that socket.

Ports:

A socket (in this code) is always bound to a specific port - if bind is
called with netx_PORT_EPHEM then the stack will pick an "ephemeral" port
which is different from all others currently in use. Because of the
issues with multicasting above, and because there is no way to
coordinate the multiple receivers of a multicast group to use a
particular ephemeral port, the same port - and therefore the same
socket, is used for all multicast reception (note a multicast packet can
be *transmitted* from any socket because it is only the destination
address which can be multicast). For example in SDT, all multicast uses
the SDT_MULTICAST_PORT.

Destination (multicast group) address:

To allow RLP to distinguish traffic for different multicast groups
Each time a higher layer registers a new listener corresponding to a
specific multicast address, we know that that listener is only concerned
with the traffic within that particular group. If the stack can provide
the incoming multicast address (the destination address from an incoming
packet we can filter) in the rlp layer and only send that packet to the
appropriate listener. Otherwise all listeners receive all multicast
packets, irrespective of their group address. They can eliminate all the
unwanted packets eventually but may need to process a considerable
amount of the packet in doing so.

The code here, does not filter incoming packets by destination address,
but if it can extract that information (STACK_RETURNS_DEST_ADDR is true)
then we do so and pass the group address on to the higher layer incoming
packet handler. NOTE: We only need to pass the group address, not a full
address and port structure, because the port (and interface if desired
subject to the limitations above) are determined by the socket.

*/


#include "opt.h"
#if CONFIG_NSK
#if CONFIG_STACK_BSD || CONFIG_STACK_CYGWIN
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

#include "netxface.h"
#include "netsock.h"
#include "ntoa.h"
#include <sys/select.h>

/************************************************************************/
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_NETX, "%s [", __func__)
#define LOG_FEND() acnlog(LOG_DEBUG | LOG_NETX, "%s ]", __func__)

/************************************************************************/
/*
NETX_SOCK_HAS_CALLBACK is only needed if multiple clients are sharing
the netsock layer. Otherwise we can call the client direct.
*/

#if NETX_SOCK_HAS_CALLBACK
#define NETX_HANDLER (*nsk->data_callback)
#elif CONFIG_RLP
extern void rlp_process_packet(netxsocket_t *socket, const uint8_t *data, int length, ip4addr_t group, netx_addr_t *source)
#define NETX_HANDLER rlp_process_packet
#elif CONFIG_SLP
extern void slp_recv(netxsocket_t *socket, const uint8_t *data, int length, ip4addr_t group, netx_addr_t *source);
#define NETX_HANDLER slp_recv
#endif /* Strange config - nothing is listening! */

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
  /* init required sub modules */
  nsk_netsocks_init();
  /* don't process twice */
  initialized_state = 1;
  return;
}


/************************************************************************/
int netx_startup(void)
{
  return OK;
}

/************************************************************************/
int netx_shutdown(void)
{
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

#define SOCKADDR struct sockaddr
#define SOCKET_ERROR -1
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
  FIXME (wrf)
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
  
  FIXME (response pn)
  What is meant by the "default" multicast interface? See notes at head
  of this file. Interface selection is only an issue for hosts with
  multiple active physical interfaces which need to multicast. Full
  handling of specific interfaces in that case will probably require
  detailed examination of routing tables - see also IP_MULTICAST_IF
  socket option.
  */

  ret =  bind(netsock->nativesock, (SOCKADDR *)&addr, sizeof(addr));

  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open: bind port %d: %s", ntohs(LCLAD_PORT(*localaddr)), strerror(errno));
    close(netsock->nativesock);
    netsock->nativesock = 0;
    return FAIL; /* FAIL */
  }

  /* save the passed in address/port number into the passed in netxsocket_s struct */
  NSK_PORT(netsock) = LCLAD_PORT(*localaddr);

  /* we will need information on destination address used */
  ret = setsockopt(netsock->nativesock, IPPROTO_IP, IP_PKTINFO, (void *)&optionOn, sizeof(optionOn));
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_open: setsockopt IP_PKTINFO: %s", strerror(errno));
    close(netsock->nativesock);
    netsock->nativesock = 0;
    return FAIL; /* FAIL */
  }

  acnlog(LOG_DEBUG | LOG_NETX, "netx_udp_open: port %d", ntohs(NSK_PORT(netsock)));

  LOG_FEND();
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
  netx_nativeSocket_t hold;

  LOG_FSTART();

  /* if it's already closed */
  if (!netsock->nativesock) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_udp_close : upd not open");
    return;
  }

  /* a little safer to mark it not used before we actually nuke it */
  hold = netsock->nativesock;
  /* clear flag that it's in use */
  netsock->nativesock = 0;
  /* close socket */
  close(hold);
  LOG_FEND();
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
  struct ip_mreq     mreq;
  int    ret;
  int    ip_ttl;
  int    opt_val;

  LOG_FSTART();

  /* if the IP passed in is not a valid multicast address */
  if (!is_multicast(local_group)) {
   return FAIL;
  }

  acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group, port: %d, group: %s", ntohs(NSK_PORT(netsock)), ntoa(local_group));

  /* socket should have some options if enabled for multicast */

  /* allow reuse of the local port number */
  ret = setsockopt(netsock->nativesock, SOL_SOCKET, SO_REUSEADDR, (void *)&optionOn, sizeof(optionOn));
  /* TODO: why does this fail? */
  #if 0
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_change_group : setsockopt: SO_REUSEADDR fail");
    return FAIL; /* fail */
  }
  #endif

  /* set the IP TTL */
  ip_ttl = 2;
  ret = setsockopt(netsock->nativesock,  IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ip_ttl,  sizeof(ip_ttl));
  if (ret == SOCKET_ERROR) {
    acnlog(LOG_WARNING | LOG_NETX, "netx_change_group : setsockopt:IP_MULTICAST_TTL fail");
    return FAIL; /* fail */
  }

  /* turn off loop back on multicast */
  ret = setsockopt(netsock->nativesock, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&optionOff, sizeof(opt_val));
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
    setsockopt(netsock->nativesock, IPPROTO_IP, IP_ADD_MEMBERSHIP,  (void *)&mreq, sizeof(mreq));
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: added");
  } else {
    setsockopt(netsock->nativesock, IPPROTO_IP, IP_DROP_MEMBERSHIP,  (void *)&mreq, sizeof(mreq));
    acnlog(LOG_DEBUG | LOG_NETX, "netx_change_group: dropped");
  }
  LOG_FEND();
  return 0; /* OK */
}

/************************************************************************/
/*
  netx_send_to()
    Send message to given address
    The call returns the number of characters sent, or negitive if an error occurred.
*/
int netx_send_to(
  netxsocket_t      *netsock,    /* contains a flag if port is open and the local port number */
  const netx_addr_t *destaddr,   /* contains dest port and ip numbers */
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

  if ((datalen = sendto(netsock->nativesock, (char *)pkt, datalen, 0, (SOCKADDR *)&dest_addr, sizeof(dest_addr))) < 0) {
    acnlog(LOG_ERR | LOG_NETX , "netx_send_to: sendto: %s", strerror(errno));
  }

  LOG_FEND();
  return datalen;
}

/************************************************************************/
/*
  Poll for input
*/
static UDPPacket recv_buffer;
int
netx_poll(void)
{
  ssize_t             length;
  fd_set              socks;
  netxsocket_t       *nsk;
  netx_nativeSocket_t high_sock = 0;
  struct timeval      timeout;  /* Timeout for select */
  int                 readsocks;
  netx_addr_t         source;
  netx_addr_t         dest;
#if STACK_RETURNS_DEST_ADDR
  uint8_t pktinfo[CMSG_SPACE(sizeof(struct in_pktinfo))];
  struct iovec bufvec[1];
  struct msghdr hdr;
  struct cmsghdr *cmp;
  ip4addr_t groupaddr;
#else
  socklen_t           addr_len
#endif

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
      FD_SET(nsk->nativesock, &socks);
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
  readsocks = select(high_sock + 1, &socks, NULL, NULL, &timeout);
  if (readsocks < 0) {
    acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: select fail: %d", errno);
    return FAIL; /* fail */
  }

  if (readsocks > 0) {
#if STACK_RETURNS_DEST_ADDR
    /* init strutures for recvmsg (man recvmsg, man cmsg for details) */
    bufvec->iov_base = recv_buffer;
    bufvec->iov_len = sizeof(UDPPacket);
    hdr.msg_name = &source;
    hdr.msg_namelen = sizeof(source);
    hdr.msg_iov = bufvec;
    hdr.msg_iovlen = 1;
    hdr.msg_control = &pktinfo;
    hdr.msg_flags = 0;
#endif

    for (nsk = nsk_first_netsock(); nsk && nsk->nativesock; nsk = nsk_next_netsock(nsk)) {
      if (FD_ISSET(nsk->nativesock, &socks)) {
        /* Get some data */
#if STACK_RETURNS_DEST_ADDR
        hdr.msg_controllen = sizeof(pktinfo);
        length = recvmsg(nsk->nativesock, &hdr, 0);
#else
        addr_len = sizeof(netx_addr_t);
        length = recvfrom(nsk->nativesock, recv_buffer, sizeof(recv_buffer), 0, (SOCKADDR *)&source, &addr_len);
#endif
        /* Test for error */
        if (length == -1) {
          acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: recvfrom fail: %d", errno);
          return FAIL; /* fail */
        }

        /* make sure we actually have some data to process */
        if (length > 0) {
#if STACK_RETURNS_DEST_ADDR
          /* Look for ancillary data of our type*/
          cmp = CMSG_FIRSTHDR(&hdr);
          do {
            if (cmp == NULL) {
              acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: unable to get ancillary data");
              return FAIL;
            }
            cmp = CMSG_NXTHDR(&hdr, cmp);
          } while (!(cmp->cmsg_level == IPPROTO_IP && cmp->cmsg_type == IP_PKTINFO));

          /* get the address and move it to our destination address*/
          groupaddr = ((struct in_pktinfo *)(CMSG_DATA(cmp)))->ipi_addr.s_addr;
          if (!is_multicast(groupaddr)) groupaddr = netx_GROUP_UNICAST;
          NETX_HANDLER(nsk, recv_buffer, length, groupaddr, &source);
#else
          NETX_HANDLER(nsk, recv_buffer, length, &source);
#endif /* STACK_RETURNS_DEST_ADDR */
        } else {
          acnlog(LOG_DEBUG | LOG_NETX , "netx_poll: length = 0");
        }
      }
    }
  }
  return OK;
}

/************************************************************************/
/*
  netx_getmyip()

*/
#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr)
{
/*
TODO: Should find the local IP address which would be used to send to
      the given remote address. For now we just get the first address
*/
  int fd;
  struct ifreq ifr;

  UNUSED_ARG(destaddr);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  close(fd);

  return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;

/* this works too */
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
*/
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr)
{
  int fd;
  struct ifreq ifr;
  UNUSED_ARG(destaddr);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
  ioctl(fd, SIOCGIFNETMASK, &ifr);
  close(fd);

  return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
}

#endif  /* CONFIG_NET_IPV4 */
#endif /* CONFIG_STACK_BSD */
#endif /* CONFIG_NSK */
