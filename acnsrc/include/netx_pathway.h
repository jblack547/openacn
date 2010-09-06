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

#if CONFIG_STACK_PATHWAY && !defined(__netx_pathway_h__)
#define __netx_pathway_h__ 1

#include "acnip.h"
#include "wattcp.h"

/************************************************************************/
/*
  We use each stack's native structure for holding transport layer (UDP)
  addresses. This information is typedef'd to netx_addr_t. To unify the
  API we hide each stacks implementation of netx_addr_t behind macros
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


typedef udp_Socket netx_nativeSocket_t;
struct netxHost_s;
typedef struct netxHost_s netx_addr_t;

/* mainain in network byte order */
struct PACKED netxHost_s {
	port_t port;
	ip4addr_t addr;
};

/* operations performed on netx_addr_s */
#define netx_PORT(addrp) (addrp)->port
#define netx_INADDR(addrp) (addrp)->addr
#define netx_INIT_ADDR_STATIC(inaddr, port) {(port), (inaddr)}
#define netx_INIT_ADDR(addrp, addr, port) ( \
      netx_INADDR(addrp) = (addr), \
      netx_PORT(addrp) = (port) \
   )

/************************************************************************/
/*
There are fairly major differences if CONFIG_LOCALIP_ANY is true (which
is the common case).
In this case we don't need to track local interface addresses at all, we
just leave it up to the stack. All we need to do is pass local port
numbers which are easy to handle. However, because of the differences in
passing conventions between ports which are integers (uint16_t) and
structures which are passed by reference, things are not so transparent
as would be nice. We define both localaddr_t (the storage type) and
localaddr_arg_t (the argument type) then use macros LCLAD_ARG(lcladdr)
and LCLAD_UNARG(lcladdrarg)
*/

#if CONFIG_LOCALIP_ANY

typedef port_t localaddr_t;
typedef localaddr_t localaddr_arg_t;

/* operations when looking at netxsock_t pointer */
#define NSK_PORT(nskptr) ((nskptr)->localaddr)
#define NSK_INADDR(nskptr) netx_INADDR_ANY

/* operations on localaddr_t */
#define netx_INIT_LOCALADDR(addrp, addr, port) (*addrp = (port))
#define LCLAD_PORT(lclad) (lclad)
#define LCLAD_INADDR(lclad) netx_INADDR_ANY
#define LCLAD_ARG(lclad) (lclad)
#define LCLAD_UNARG(lclad) (lclad)

#else /* !CONFIG_LOCALIP_ANY */

typedef netx_addr_t localaddr_t;
typedef localaddr_t *localaddr_arg_t;

/* operations on netxsock_t */
#define NSK_PORT(nskptr) netx_PORT(&(nskptr)->localaddr)
#define NSK_INADDR(nskptr) netx_INADDR(&(nskptr)->localaddr)

/* operations on localaddr_t */
#define netx_INIT_LOCALADDR(addrp, addr, port) netx_INIT_ADDR(addrp, addr, port)
#define LCLAD_PORT(laddr) netx_PORT(&(laddr))
#define LCLAD_INADDR(laddr) netx_INADDR(&(laddr))
#define LCLAD_ARG(lclad) (&(lclad))
#define LCLAD_UNARG(lclad) (*(lclad))

#endif /* !CONFIG_LOCALIP_ANY */

/************************************************************************/
/* operation argument for netx_change_group */
#define netx_JOINGROUP 1
#define netx_LEAVEGROUP 0

/************************************************************************/
#if CONFIG_NET_IPV4
ip4addr_t netx_getmyip(netx_addr_t *destaddr);
ip4addr_t netx_getmyipmask(netx_addr_t *destaddr);

#define netx_getmyip(destaddr) (my_ip_addr)

#endif /* CONFIG_NET_IPV4 */

/************************************************************************/
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


#endif	/* #if CONFIG_STACK_PATHWAY && !defined(__netx_pathway_h__) */
