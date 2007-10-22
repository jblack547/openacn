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

#if !defined(__opt_h__)
#define __opt_h__ 1
/*
  IMPORTANT
  YOU SHOULD NOT NEED TO EDIT THIS HEADER
  
  If you just want to create your own tailored build of OpenACN you should
  put all your local configuration options into the header "user_opt.h" where
  the compiler will find it.
  
  This header (opt.h) includes your user_opt.h first and only provides default values if
  options have not been defined there.
  
  You can refer to this header to see which options are available and what 
  they do.
*/

#include "user_opt.h"
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

/*
  Basic Architecture

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

/*
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
#ifndef CONFIG_NETAPI_BSD
#define	CONFIG_NETAPI_BSD       1
#endif
/* Winsock sockets */
#ifndef CONFIG_NETAPI_WINSOCK
#define	CONFIG_NETAPI_WINSOCK   0
#endif
/* Waterloo stack */
#ifndef CONFIG_NETAPI_WATERLOO
#define	CONFIG_NETAPI_WATERLOO  0
#endif

/*
  memory management models - pick one
*/
#ifndef CONFIG_RLPMEM_MALLOC
#define CONFIG_RLPMEM_MALLOC  0
#endif
#ifndef CONFIG_RLPMEM_STATIC
#define	CONFIG_RLPMEM_STATIC   1
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
  Set to zero for generic SDT. However, efficiency gains can
  be made if SDT is built for only one client protocol.
  In this case set CONFIG_SDT_SINGLE_CLIENT to the protocol ID
  of that client e.g.
  #ifndef CONFIG_SDT_SINGLE_CLIENT
#define  CONFIG_SDT_SINGLE_CLIENT PROTO_DMP
#endif
*/
#ifndef CONFIG_SDT_SINGLE_CLIENT
#define CONFIG_SDT_SINGLE_CLIENT 0
#endif

/***************************************************************************************/
/*
  The following are sanity checks on preceding options and some
  derivative configuration values. They are not user options
*/
/***************************************************************************************/

/* check on transport selection */
#if (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) <= 0
#error Need to select at least one transport
#elif (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) == 1
#define CONFIG_MULTI_NET 0
#else
#define CONFIG_MULTI_NET 1
#endif

/* check on network stack */
#if (CONFIG_NETAPI_BSD + CONFIG_NETAPI_WINSOCK + CONFIG_NETAPI_WATERLOO) != 1
#error Need to select exactly one network stack
#endif

/* Sanity check for CONFIG_RLP_SINGLE_CLIENT */
#if ((CONFIG_SDT + CONFIG_E131) > 1 && (CONFIG_RLP_SINGLE_CLIENT) != 0)
#error Cannot support both SDT and E1.31 if CONFIG_RLP_SINGLE_CLIENT is set
#endif

#endif
