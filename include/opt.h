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

/*
  Inline functions for marshaling data are efficient and typecheck
  the code. If the compiler supports inline code then they are
  preferable.

  If you do not want to compile inline, then setting this false
  uses macros instead, but these eveluate their arguments multiple times
  and do not check their types so beware.
*/
#ifndef CONFIG_MARSHAL_INLINE
#define CONFIG_MARSHAL_INLINE 1
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
#define	CONFIG_STACK_BSD       0
#endif
/* Winsock sockets */
#ifndef CONFIG_STACK_WINSOCK
#define	CONFIG_STACK_WINSOCK   0
#endif
/* LightweightIP (LWIP) stack */
#ifndef CONFIG_STACK_LWIP
#define	CONFIG_STACK_LWIP     0
#endif
/* Pathway Connectivity stack - derived from Waterloo stack */
#ifndef CONFIG_STACK_PATHWAY
#define	CONFIG_STACK_PATHWAY  0
#endif
/* Netburner sockets */
#ifndef CONFIG_STACK_NETBURNER
#define CONFIG_STACK_NETBURNER 0
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
  #define MAX_LISTENERS 50
  #endif
  #ifndef MAX_RXGROUPS
  #define MAX_RXGROUPS 50
  #endif
  #ifndef MAX_TXBUFS
  #define MAX_TXBUFS 10
  #endif
#endif

#ifndef MAX_NSK_SOCKETS
#define MAX_NSK_SOCKETS 50
#endif

/************************************************************************/
/*
  Logging

  Set CONFIG_ACNLOG to determine how messages are logged.
  Set CONFIG_LOGLEVEL to determine what level of messages are logged.

  CONFIG_ACNLOG options are:

  ACNLOG_NONE		//Logging is compiled out
  ACNLOG_SYSLOG		//Log using POSIX Syslog
  ACNLOG_STDOUT		//Log to standard output (default)
  ACNLOG_STDERR		//Log to standard error

  Syslog handles logging levels itself and CONFIG_LOGLEVEL is ignored.
  For other options Messages up to CONFIG_LOGLEVEL are logged & levels
  beyond this are ignored. Possible values are (in increasing order).

  LOG_EMERG
  LOG_ALERT
  LOG_CRIT
  LOG_ERR
  LOG_WARNING
  LOG_NOTICE
  LOG_INFO
  LOG_DEBUG

  The syslog() macro is formated to match the POSIX version:
    extern void syslog(int, const char *, ...);
  Where int is the combination of facility and error level (or'd),
  const * is a formating string and ... is a list of argument.
  This allows for a function simialr to the standard printf

  The normal facility values have been extended with LOG_NONE which
  will disable logging. This allows module level control.

  Log are disable by default (LOG_NONE).  To enable them, changed the module
  level define to the desired facility in user_opt.h

  #define RLP_LOG LOCAL0

  Then to send messages:

    acn_log(RLP_LOG, "I got an error")'
    anc_log(RLP_LOG, "I got %d errors", error_count);

  Log levels can still be added:
    acn_log(RLP_LOG | LOG_INFO, "I do not like errors");
  and would only print if CONFIG_LOGLEVEL was LOG_INFO or higher.
*/
#ifndef CONFIG_ACNLOG
#define CONFIG_ACNLOG ACNLOG_STDOUT
#endif

#ifndef CONFIG_LOGLEVEL
#define CONFIG_LOGLEVEL LOG_CRIT
#endif

#define LOG_NONE (-1)
#ifndef LOG_RLP
  #define LOG_RLP LOG_NONE
#endif
#ifndef LOG_RLPM
  #define LOG_RLPM LOG_NONE
#endif
#ifndef LOG_SDT
  #define LOG_SDT LOG_NONE
#endif
#ifndef LOG_SDTM
  #define LOG_SDTM LOG_NONE
#endif
#ifndef LOG_NSK
  #define LOG_NSK LOG_NONE
#endif
#ifndef LOG_NETX
  #define LOG_NETX LOG_NONE
#endif
#ifndef LOG_SLP
  #define LOG_SLP LOG_NONE
#endif
#ifndef LOG_DMP
  #define LOG_DMP LOG_NONE
#endif
#ifndef LOG_MISC
  #define LOG_MISC LOG_NONE
#endif
#ifndef LOG_STAT
  #define LOG_STAT LOG_NONE
#endif
#ifndef LOG_ASSERT
  #define LOG_ASSERT LOG_NONE
#endif



/************************************************************************/
/*
  ACN Protocols
*/
/*
Protocols to build
*/
#ifndef CONFIG_SDT
#define CONFIG_SDT	   0
#endif
#ifndef CONFIG_DMP
#define CONFIG_DMP     0
#endif
#ifndef CONFIG_E131
#define CONFIG_E131    0
#endif
#ifndef CONFIG_SLP
#define CONFIG_SLP     0
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

/* See EIP 19, standard allows these to be bigger but to keep memory usage and avoid malloc */
/* we are making them static. Default of 64 should work (see EPI 19, section 3.2) */
#ifndef ACN_FCTN_SIZE
#define ACN_FCTN_SIZE 64  /* be sure to include one for a null terminator */
#endif
#ifndef ACN_UACN_SIZE
#define ACN_UACN_SIZE 64  /* be sure to include one for a null terminator */
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

#ifndef CONFIG_SDTMEM_MALLOC
#define CONFIG_SDTMEM_MALLOC  0
#endif

#ifndef CONFIG_SDTMEM_STATIC
#define	CONFIG_SDTMEM_STATIC   1
#endif

/* Configures the limit to for SDT  */
#ifndef SDT_MAX_COMPONENTS
#define SDT_MAX_COMPONENTS          4
#endif

#ifndef SDT_MAX_CHANNELS
#define SDT_MAX_CHANNELS            4
#endif

#ifndef SDT_MAX_MEMBERS
#define SDT_MAX_MEMBERS             4
#endif

#ifndef SDT_MAX_HANDLERS
#define SDT_MAX_HANDLERS            4
#endif

#ifndef SDT_MAX_SESSIONS
#define SDT_MAX_SESSIONS            4
#endif

/* Must be less than MAX_TXBUFS */
#ifndef SDT_MAX_RESENDS
#define SDT_MAX_RESENDS             4
#endif

#ifndef SDT_RESEND_TIMEOUT_ms
#define SDT_RESEND_TIMEOUT_ms      5000
#endif

/************************************************************************/
/*

  DMP
*/
#ifndef DMP_MAX_SUBSCRIPTIONS
#define DMP_MAX_SUBSCRIPTIONS       100
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
#if (CONFIG_STACK_BSD + CONFIG_STACK_WINSOCK + CONFIG_STACK_PATHWAY + CONFIG_STACK_LWIP + CONFIG_STACK_NETBURNER) != 1
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
//TODO: wrf combine these into one define?
// Sanity check for memory RLP managment
#if (CONFIG_RLPMEM_MALLOC + CONFIG_RLPMEM_STATIC) != 1
#error Need to select one RLP memory managment model
#endif
// Sanity check for memory SDT managment
#if (CONFIG_SDTMEM_MALLOC + CONFIG_SDTMEM_STATIC) != 1
#error Need to select one SDT memory managment model
#endif


#endif
