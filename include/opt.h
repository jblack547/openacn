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

#ifndef __opt_h__
#define __opt_h__ 1
#include "user_opt.h"

/**************************************************************************/
/*
  IMPORTANT
  YOU SHOULD NOT NEED TO EDIT THIS HEADER
  
  If you just want to create your own tailored build of OpenACN you
  should put all your local configuration options into the header
  "user_opt.h" where the compiler will find it.

  This header (opt.h) includes your user_opt.h first and only provides
  default values if options have not been defined there.

  You can refer to this header to see which options are available and
  what  they do.
*/
/**************************************************************************/

/**************************************************************************/
/*
  C Compiler
  
  Sort this out first since a lot else depends on it

  we can autodetect this from predefined macros
  but these vary from system to system so use our own macro 
  names and allow user override in user_opt.h
*/

#ifndef CONFIG_GNUCC
#ifdef __GNUC__
#define CONFIG_GNUCC 1
#else
#define CONFIG_GNUCC 0
#endif
#endif

/************************************************************************/
/*
  Basic CPU Architecture

  we can autodetect a lot of this from predefined macros
  but these vary from system to system so use our own macro 
  names and allow user override in user_opt.h
*/
#ifndef ARCH_AMD64
#ifdef __amd64__
#define ARCH_AMD64 1
#else
#define ARCH_AMD64 0
#endif
#endif

#ifndef ARCH_i386
#ifdef __i386__
#define ARCH_X86 1
#else
#define ARCH_X86 0
#endif
#endif

#ifndef ARCH_H8300
#ifdef __H8300__
#define ARCH_H8300 1
#else
#define ARCH_H8300 0
#endif
#endif

#ifndef ARCH_H8300S
#ifdef __H8300S__
#define ARCH_H8300S 1
#else
#define ARCH_H8300S 0
#endif
#endif

/************************************************************************/
/*
  Networking

  Underlying transport selection
  picking more than one makes code more complex
*/
/* Internet Protocol v4 */
#ifndef CONFIG_NET_IPV4
#define	CONFIG_NET_IPV4	1
#endif

/* Internet Protocol v6 */
#ifndef CONFIG_NET_IPV6
#define	CONFIG_NET_IPV6	0
#endif

/*
  Network stack and API to use - pick just one
*/
/* BSD sockets */
#ifndef CONFIG_STACK_BSD
#define	CONFIG_STACK_BSD       1
#endif
/* Winsock sockets */
#ifndef CONFIG_STACK_WINSOCK
#define	CONFIG_STACK_WINSOCK   0
#endif
/* LightweightIP (LWIP) stack */
#ifndef CONFIG_STACK_LWIP
#define	CONFIG_STACK_LWIP  0
#endif
/* Pathway Connectivity stack - derived from Waterloo stack */
#ifndef CONFIG_STACK_PATHWAY
#define	CONFIG_STACK_PATHWAY  0
#endif

/*
In hosts with multiple interfaces (including the loopback interface) it
is normal to accept incoming packets for any interface (local address)
and to leave it to the stack to select the interface for outgoing
packets (in BSD this is done by binding sockets to INADDR_ANY).

If CONFIG_LOCALIP_ANY is set, RLP and SDT rely
entirely on the stack to handle IP addresses and interfaces and the API
does not allow local addresses to be specified (except for multicast) and
only stores port information. This saves using resources tracking
addresses which are always unspecified.

If CONFIG_LOCALIP_ANY is false then the API allows higher layers to
specify individual interfaces at the expense of slightly more code and
memory. This setting still allows the value NETI_INADDR_ANY to be used
as required.
*/
#ifndef CONFIG_LOCALIP_ANY
#define	CONFIG_LOCALIP_ANY       1
#endif

/************************************************************************/
/*
  Memory management
  
  RLP models - pick one
*/
#ifndef CONFIG_RLPMEM_MALLOC
#define CONFIG_RLPMEM_MALLOC  0
#endif
#ifndef CONFIG_RLPMEM_STATIC
#define	CONFIG_RLPMEM_STATIC   1
#endif

#if CONFIG_RLPMEM_STATIC
  #ifndef MAX_RLP_SOCKETS
  #define MAX_RLP_SOCKETS 50
  #endif
  #ifndef MAX_LISTENERS
  #define MAX_LISTENERS 100
  #endif
  #ifndef MAX_TXBUFS
  #define MAX_TXBUFS 10
  #endif
#endif

/************************************************************************/
/*
  ACN Protocols
*/
/*
Protocols to build
*/
#ifndef CONFIG_SDT
#define CONFIG_SDT	   1
#endif
#ifndef CONFIG_DMP
#define CONFIG_DMP     0
#endif
#ifndef CONFIG_E131
#define CONFIG_E131    0
#endif

/*
  EPI conformance
*/
#ifndef CONFIG_EPI10
#define	CONFIG_EPI10   1
#endif
#ifndef CONFIG_EPI11
#define	CONFIG_EPI11   1
#endif
#ifndef CONFIG_EPI12
#define	CONFIG_EPI12   1
#endif
#ifndef CONFIG_EPI13
#define	CONFIG_EPI13   1
#endif
#ifndef CONFIG_EPI15
#define	CONFIG_EPI15   1
#endif
#ifndef CONFIG_EPI16
#define	CONFIG_EPI16   1
#endif
#ifndef CONFIG_EPI17
#define	CONFIG_EPI17   1
#endif
#ifndef CONFIG_EPI18
#define	CONFIG_EPI18   1
#endif
#ifndef CONFIG_EPI19
#define	CONFIG_EPI19   1
#endif
#ifndef CONFIG_EPI20
#define	CONFIG_EPI20   1
#endif

/************************************************************************/
/*
  Component Model
  
  If there is only ever one component in the host using openACN things
  get simpler.
*/
#ifndef CONFIG_SINGLE_COMPONENT
#define	CONFIG_SINGLE_COMPONENT   1
#endif


/************************************************************************/
/*

  Root Layer Protocol

  The default is to build a generic RLP for multiple client protocols.
  However, efficiency gains can be made if RLP is built for only one,
  in this case set CONFIG_RLP_SINGLE_CLIENT to the protocol ID
  of that client (in user_opt.h) e.g.
  #define CONFIG_RLP_SINGLE_CLIENT PROTO_SDT
*/
#ifndef CONFIG_RLP_SINGLE_CLIENT
#define CONFIG_RLP_SINGLE_CLIENT 0
#endif

/*
  If we want RLP to optimize packing for size (instead of handling speed)
  define this true
*/
#ifndef CONFIG_RLP_OPTIMIZE_PACK
#define CONFIG_RLP_OPTIMIZE_PACK 0
#endif

/************************************************************************/
/*

  SDT

  Set to zero for generic SDT. However, efficiency gains can
  be made if SDT is built for only one client protocol.
  In this case set CONFIG_SDT_SINGLE_CLIENT to the protocol ID
  of that client e.g:
    #ifndef CONFIG_SDT_SINGLE_CLIENT
    #define  CONFIG_SDT_SINGLE_CLIENT PROTO_DMP
    #endif
*/
#ifndef CONFIG_SDT_SINGLE_CLIENT
#define CONFIG_SDT_SINGLE_CLIENT 0
#endif

/************************************************************************/
/*
  The following are sanity checks on preceding options and some
  derivative configuration values. They are not user options
*/
/************************************************************************/

/************************************************************************/
/* check on transport selection */
#if (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) <= 0
#error Need to select at least one transport
#elif (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) == 1
#define CONFIG_MULTI_NET 0
#else
#define CONFIG_MULTI_NET 1
#endif

/************************************************************************/
/* checks on network stack */
#if (CONFIG_STACK_BSD + CONFIG_STACK_WINSOCK + CONFIG_STACK_PATHWAY + CONFIG_STACK_LWIP) != 1
#error Need to select exactly one network stack
#endif

#if CONFIG_STACK_PATHWAY && (!CONFIG_NET_IPV4 || CONFIG_MULTI_NET)
#error Pathway stack only supports IPv4
#endif

/************************************************************************/
/* Sanity check for CONFIG_RLP_SINGLE_CLIENT */
#if ((CONFIG_SDT + CONFIG_E131) > 1 && (CONFIG_RLP_SINGLE_CLIENT) != 0)
#error Cannot support both SDT and E1.31 if CONFIG_RLP_SINGLE_CLIENT is set
#endif

/************************************************************************/
/* Sanity check for memory managment */
#if (CONFIG_RLPMEM_MALLOC +  CONFIG_RLPMEM_STATIC) != 1
#error Need to select one memory managment model
#endif


#endif
