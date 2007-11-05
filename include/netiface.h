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

#if defined(CONFIG_NET_IPV4)

typedef uint16_t port_t;	/* net endpoint is a port */
typedef uint32_t ip4addr_t;	/* net group is a multicast address */
typedef ip4addr_t groupaddr_t
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

#if defined(CONFIG_STACK_BSD)

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

typedef int neti_nativeSocket_t;

typedef struct sockaddr_in netiHost_t;

typedef struct netsocket_s {
	neti_nativeSocket_t nativesock;
	port_t localPort;
};

int neti_udp_open(struct netsocket_s *netsock, port_t localPort);
void neti_udp_close(struct netsocket_s *netsock);
int neti_change_group(struct netsocket_s *netsock, ip4addr_t localGroup, bool add);

#define neti_join_group(netsock, group) neti_change_group(netsock, group, 1)
#define neti_leave_group(netsock, group) neti_change_group(netsock, group, 0)

#elif defined(CONFIG_STACK_WATERLOO)

#include "wattcp.h"

typedef udp_Socket neti_nativeSocket_t;

/* mainain in network byte order */
struct PACKED netiHost_s {
	port_t port;
	ip4addr_t addr;
};

typedef struct netiHost_s netiHost_t;

typedef struct netsocket_s {
	neti_nativeSocket_t nativesock;
	port_t localPort;
};

int neti_udp_open(struct netsocket_s *netsock, port_t localPort);
void neti_udp_close(struct netsocket_s *netsock);
int neti_join_group(struct netsocket_s *netsock, ip4addr_t localGroup);
int neti_leave_group(struct netsocket_s *netsock, ip4addr_t localGroup);


#endif

#if defined(CONFIG_SDT)
/*
SDT packets use a standard transport address format for address and port which may differ from native format
*/

#if defined(CONFIG_STACK_BSD)

#include "acn_sdt.h"

/* Both native and ACN formats are network byte order */
extern __inline__ void transportAddrToHost(const uint8_t *transaddr, netiHost_t *hostaddr)
{
	hostaddr->sin_family = AF_INET;
	memcpy(&hostaddr->sin_port, transaddr+1, 2);
	memcpy(&hostaddr->sin_addr, transaddr+3, 4);
}

/* Both native and ACN formats are network byte order */
extern __inline__ void hostToTransportAddr(const netiHost_t *hostaddr, uint8_t *transaddr)
{
	*transaddr = SDT_ADDR_IPV4;
	memcpy(transaddr+1, &hostaddr->sin_port, 2);
	memcpy(transaddr+3, &hostaddr->sin_addr, 4);
}

#elif defined(CONFIG_STACK_WATERLOO)

/* Both native and ACN formats are network byte order */
extern __inline__ void transportAddrToHost(const uint8_t *transaddr, netiHost_t *hostaddr)
{
	memcpy(&hostaddr, transaddr+1, 6);
}

/* Both native and ACN formats are network byte order */
extern __inline__ void hostToTransportAddr(const netiHost_t *hostaddr, uint8_t *transaddr)
{
	*transaddr = SDT_ADDR_IPV4;
	memcpy(transaddr+1, &hostaddr, 6);
}

#endif	/* #if defined(CONFIG_STACK_BSD) */
#endif	/* #if defined(CONFIG_SDT) */
#endif	/* #if defined(CONFIG_NET_IPV4) */
#endif	/* #ifndef __netiface_h__ */
