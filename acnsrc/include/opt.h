/************************************************************************/
/*
  Copyright (c) 2007-2010, Engineering Arts (UK)

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

#tabs=2s
*/
/**************************************************************************/

#ifndef __opt_h__
#define __opt_h__ 1
#include "user_opt.h"

/************************************************************************/
/*
  IMPORTANT
  YOU SHOULD NOT NEED TO EDIT THIS HEADER

  If you just want to create your own tailored build of OpenACN you
  should put all your local configuration options into the header
  "user_opt.h" where the compiler will find it.

  This header (opt.h) includes your user_opt.h first and only provides
  default values if options have not been defined there.

  You can refer to this header to see which options are available and
  what  they do. Note that options may not be implemented, may  only
  work for certain builds or may only work in specific combinations.
*/
/************************************************************************/

/************************************************************************/
/*
ACN Version
ACN was revised in 2010 and changes or corrections were made to: SDT,
DMP, DDL, EPI10, EPI11, EPI18, EPI19, EPI22.

ACN_VERSION is an integer which represents the ACN revision to be
compiled.

EPIs which were not included in the original ACN suite have their own
standardization process and will need their own version numbers as
necessary.

Expect earlier versions of ACN to become deprecated over time

Allowable values are:
20060000    the original ANSI ESTA E1.17-2006 version
20100000    the revised version ANSI ESTA E1.17-2010 (pending final approval)

*/
/************************************************************************/

#ifndef ACN_VERSION
#define ACN_VERSION 20100000
#endif

/************************************************************************/
/*
  ACN Protocols

  Define which protocols and EPIs to build or conform to
  
  NSK layer is not an ACN protocol but has a switch here as it is the
  lowest layer in openacn.

  Default all on except:
    E1.31 - not complete yet
    EPI13 - superseded by EPI29
*/
/************************************************************************/

#ifndef CONFIG_NSK
#define CONFIG_NSK     1
#endif
/*
  Core protocols
*/
#ifndef CONFIG_RLP
#define CONFIG_RLP     1
#endif
#ifndef CONFIG_SDT
#define CONFIG_SDT     1
#endif
#ifndef CONFIG_DMP
#define CONFIG_DMP     1
#endif
#ifndef CONFIG_E131
#define CONFIG_E131    0
#endif
#ifndef CONFIG_SLP
#define CONFIG_SLP     1
#endif

/*
  EPI conformance
*/
#ifndef CONFIG_EPI10
#define  CONFIG_EPI10   1
#endif
#ifndef CONFIG_EPI11
#define  CONFIG_EPI11   1
#endif
#ifndef CONFIG_EPI12
#define  CONFIG_EPI12   1
#endif
#ifndef CONFIG_EPI13
#define  CONFIG_EPI13   0
#endif
#ifndef CONFIG_EPI15
#define  CONFIG_EPI15   1
#endif
#ifndef CONFIG_EPI16
#define  CONFIG_EPI16   1
#endif
#ifndef CONFIG_EPI17
#define  CONFIG_EPI17   1
#endif
#ifndef CONFIG_EPI18
#define  CONFIG_EPI18   1
#endif
#ifndef CONFIG_EPI19
#define  CONFIG_EPI19   1
#endif
#ifndef CONFIG_EPI20
#define  CONFIG_EPI20   1
#endif
#ifndef CONFIG_EPI29
#define  CONFIG_EPI29   1
#endif

/************************************************************************/
/*
  C Compiler

  Sort this out first since a lot else depends on it

  we can autodetect this from predefined macros
  but these vary from system to system so use our own macro
  names and allow user override in user_opt.h
*/
/************************************************************************/

/* Best to use GCC if available */
#ifndef CONFIG_GNUCC
#ifdef __GNUC__
#define CONFIG_GNUCC 1
#else
#define CONFIG_GNUCC 0
#endif
#endif

/* MS Visual C - only for Windows native targets */
#ifndef CONFIG_MSVC
#ifdef _MSC_VER
#define CONFIG_MSVC _MSC_VER
#else
#define CONFIG_MSVC 0
#endif
#endif

/**************************************************************************/
/*
  Basic CPU Architecture

  we can autodetect a lot of this from predefined macros
  but these vary from system to system so use our own macro
  names and allow user override in user_opt.h
*/
/************************************************************************/

#ifndef ARCH_x86_64
#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_x86_64 1
#else
#define ARCH_x86_64 0
#endif
#endif

#ifndef ARCH_i386
#if defined(__i386__) || defined(_M_IX86)
#define ARCH_i386 1
#else
#define ARCH_i386 0
#endif
#endif

#ifndef ARCH_h8300
#if defined(__H8300__)
#define ARCH_h8300 1
#else
#define ARCH_h8300 0
#endif
#endif

#ifndef ARCH_h8300s
#if defined(__H8300S__)
#define ARCH_h8300s 1
#else
#define ARCH_h8300s 0
#endif
#endif

#ifndef ARCH_thumb
#if defined(__thumb__)
#define ARCH_thumb 1
#else
#define ARCH_thumb 0
#endif
#endif

#ifndef ARCH_arm
#if defined(__arm__)
#define ARCH_arm 1
#else
#define ARCH_arm 0
#endif
#endif

#ifndef ARCH_coldfire
#if defined(__mcoldfire__)
#define ARCH_coldfire 1
#else
#define ARCH_coldfire 0
#endif
#endif

#ifndef ARCH_m68k
#if defined(__m68k__)
#define ARCH_m68k 1
#else
#define ARCH_m68k 0
#endif
#endif

/**************************************************************************/
/*
  Standard type names (e.g. uint16_t etc.)

  These are standard names in ISO C99 defined in inttypes.h. If your
  compiler is C99 compliant or nearly so, it should pick this up
  automatically.

  For archaic ISO C89 compilers (Windows et al) it will attempt to
  generate these types using typefromlimits.h (openacn specific) and
  C89 standard header limits.h.

  If your compiler is not C99 compliant but nevertheless has a good
  inttypes.h header available, then define HAVE_INT_TYPES_H to 1

  Finally if you are providing your own definitions for these types.
  Define USER_DEFINE_INTTYPES to 1 and provide your own definitions in
  user_types.h. (Examples are provided).

  Leaving the compiler to sort it out from default values is likely to
  be far more portable, but defining your own may be cleaner and
  easier for deeply embedded builds.

  See acnstdtypes.h for more info.
*/
/************************************************************************/

#ifndef USER_DEFINE_INTTYPES
#define USER_DEFINE_INTTYPES 0
#endif

/************************************************************************/
/*
  Networking
*/
/************************************************************************/

/*
  Underlying transport selection
  picking more than one makes code more complex
*/

/* Internet Protocol v4 */
#ifndef CONFIG_NET_IPV4
#define  CONFIG_NET_IPV4  1
#endif

/* Internet Protocol v6 */
#ifndef CONFIG_NET_IPV6
#define  CONFIG_NET_IPV6  0
#endif

/*
  Network stack and API to use - pick just one
*/

/* BSD sockets */
#ifndef CONFIG_STACK_BSD
#define  CONFIG_STACK_BSD       0
#endif
/* Winsock sockets */
#ifndef CONFIG_STACK_WIN32
#define  CONFIG_STACK_WIN32   0
#endif
/* LightweightIP (LWIP) stack */
#ifndef CONFIG_STACK_LWIP
#define  CONFIG_STACK_LWIP     0
#endif
/* Pathway Connectivity stack - derived from Waterloo stack */
#ifndef CONFIG_STACK_PATHWAY
#define  CONFIG_STACK_PATHWAY  0
#endif
/* Netburner sockets */
#ifndef CONFIG_STACK_NETBURNER
#define CONFIG_STACK_NETBURNER 0
#endif
/* Cygwin sockets */
#ifndef CONFIG_STACK_CYGWIN
#define  CONFIG_STACK_CYGWIN    0
#endif

/*
  Filter by incoming address

  If the stack has the ability to return the (multicast) destination
  address then RLP will make sure that callbacks to STD are filtered by
  the desired multicast address. Otherwise, the filtering is only done
  by port and callbacks to the same socket will get all socket messages
  regardless of the mulitcast address registered. These messages will
  ultimately be rejected by higher layers of code, but only after wading
  through the contents of the packet multiple times! If you can possibly
  work out how to extract the multicast destination address from an
  incoming packet you should do so. See platform/linux.netxface.c for
  discussion of issues with multicast addresses.
*/

#ifndef STACK_RETURNS_DEST_ADDR
#if (CONFIG_STACK_CYGWIN || CONFIG_STACK_PATHWAY)
/*
These are broken stacks!
*/
#define STACK_RETURNS_DEST_ADDR 0
#else
#define STACK_RETURNS_DEST_ADDR 1
#endif
#endif

/*
  Allow code to specify interfaces

  In hosts with multiple interfaces (including the loopback interface)
  it is normal to accept packets received on any interface and to leave
  it to the stack to select the interface for outgoing packets - in BSD
  this is done by binding sockets to INADDR_ANY, other stacks have
  similar mechanisms, or may even be incapable of any other behavior.

  If CONFIG_LOCALIP_ANY is set, RLP and SDT rely entirely on the stack
  to handle IP addresses and interfaces, the API does not allow local
  interfaces to be specified and only stores port information. This
  saves using resources tracking redundant interface information.

  If CONFIG_LOCALIP_ANY is false then the API allows higher layers to
  specify individual interfaces (by their address) at the expense of
  slightly more code and memory. This setting still allows the value
  NETI_INADDR_ANY to be used as required.
*/

#ifndef CONFIG_LOCALIP_ANY
#define  CONFIG_LOCALIP_ANY       1
#endif

/************************************************************************/
/*
  Memory management

  Low level routines can allocate memory either using malloc system
  calls which is flexible but less predictable and depending on the
  underlying system, may be slower, or from pre-assigned buffers which
  is inflexible and can be wasteful but is deterministic and can be
  faster.

  Normally you just need to define CONFIG_MEM to either MEM_STATIC or
  MEM_MALLOC. For fine tuning of different protocols, the macros
  CONFIG_RLPMEM, CONFIG_SDTMEM etc. can be assigned separately.
*/
/************************************************************************/

#define MEM_STATIC 1
#define MEM_MALLOC 2

#ifndef CONFIG_MEM
#define CONFIG_MEM MEM_STATIC
#endif

/**************************************************************************/
/*
  Logging

  Set CONFIG_ACNLOG to determine how messages are logged.
  Set CONFIG_LOGLEVEL to determine what level of messages are logged.

  CONFIG_ACNLOG options are:

  ACNLOG_NONE    - Logging is compiled out
  ACNLOG_SYSLOG    - Log using POSIX Syslog
  ACNLOG_STDOUT    - Log to standard output (default)
  ACNLOG_STDERR    - Log to standard error

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
/************************************************************************/

#ifndef CONFIG_ACNLOG
#define CONFIG_ACNLOG ACNLOG_STDOUT
#endif

#ifndef CONFIG_LOGLEVEL
#define CONFIG_LOGLEVEL LOG_CRIT
#endif

#define LOG_NONE (-1)

#ifndef LOG_COMPM
  #define LOG_COMPM LOG_NONE
#endif
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
#ifndef LOG_DISC
  #define LOG_DISC LOG_NONE
#endif
#ifndef LOG_DMP
  #define LOG_DMP LOG_NONE
#endif
#ifndef LOG_DMPM
  #define LOG_DMPM LOG_NONE
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
#ifndef LOG_E131
  #define LOG_E131 LOG_NONE
#endif

/**************************************************************************/
/*
  Component Model
*/
/************************************************************************/
/*
  Multiple components

  If there is only ever one component in the host using openACN things
  can be simplified.
*/
#ifndef CONFIG_SINGLE_COMPONENT
#define  CONFIG_SINGLE_COMPONENT   1
#endif

/*
  Component name strings

  Fixed Component Type Name (FCTN) and User Assigned Component Name
  (UACN) Defined in EPI19, these are transmitted in UTF-8 encoding

  The standard does not specify a size for FCTN so we arbirarily assign
  storage.

  The standard specifies a minimum storage of 63 characters for UACN
  which requires 189 bytes if stored as UTF-8. Storing as UTF-16 would
  require less storage but more processing.
*/

#ifndef ACN_FCTN_SIZE
#define ACN_FCTN_SIZE 128  /* arbitrary */
#endif
#ifndef ACN_UACN_SIZE
#define ACN_UACN_SIZE 190  /* allow for null terminator */
#endif

/**************************************************************************/
/*
  Data Marshalling

  Inline functions for marshaling data are efficient and typecheck
  the code. If the compiler supports inline code then they are
  preferable.

  If you do not want to compile inline, then setting this false
  uses macros instead, but these eveluate their arguments multiple times
  and do not check their types so beware.
*/
/************************************************************************/

#ifndef CONFIG_MARSHAL_INLINE
#define CONFIG_MARSHAL_INLINE 1
#endif

/************************************************************************/
/*
  Component tracking
*/
/************************************************************************/

/* Default CONFIG_COMPONENTMEM to follow CONFIG_MEM */

#ifndef CONFIG_COMPONENTMEM
#define CONFIG_COMPONENTMEM CONFIG_MEM
#endif

#ifndef CONFIG_MAX_COMPONENTS
#define CONFIG_MAX_COMPONENTS    4
#endif

/************************************************************************/
/*
  Network socket layer

  this is a layer to splice RLP to your stack and OS. it is highly
  platform specific and these parameters may not be used at all.
*/
/************************************************************************/

#ifndef MAX_NSK_SOCKETS
#define MAX_NSK_SOCKETS 50
#endif

/************************************************************************/
/*
  Root Layer Protocol
*/
/************************************************************************/
#if CONFIG_RLP

/*
  The default is to build a generic RLP for multiple client protocols.
  However, efficiency gains can be made if RLP is built for only one
  client protocol (probably SDT or E1.31), in this case set
  CONFIG_RLP_SINGLE_CLIENT to 1 and define the the protocol ID of that
  client (in user_opt.h) as CONFIG_RLP_CLIENTPROTO
  
  e.g. For E1.31 only support
    #define CONFIG_RLP_SINGLE_CLIENT 1
    #define CONFIG_RLP_CLIENTPROTO E131_PROTOCOL_ID
*/
#ifndef CONFIG_RLP_SINGLE_CLIENT
#define CONFIG_RLP_SINGLE_CLIENT 0
#endif

#if CONFIG_RLP_SINGLE_CLIENT && !defined(CONFIG_RLP_CLIENTPROTO)
#define CONFIG_RLP_CLIENTPROTO SDT_PROTOCOL_ID
#endif

/*
  RLP memory management - default to global model
*/
#ifndef CONFIG_RLPMEM
#define CONFIG_RLPMEM CONFIG_MEM
#endif

#if (CONFIG_RLPMEM == MEM_STATIC)
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

/*
  If we want RLP to optimize PDU packing for size of packets (instead of
  handling speed) define CONFIG_RLP_OPTIMIZE_PACK true.
*/
#ifndef CONFIG_RLP_OPTIMIZE_PACK
#define CONFIG_RLP_OPTIMIZE_PACK 0
#endif

#endif  /* CONFIG_RLP */

/**************************************************************************/
/*
  SDT
*/
/************************************************************************/
#if CONFIG_SDT

/*
  Client protocols

  The default is to build a generic SDT for multiple client protocols.
  However, efficiency gains can be made if SDT is built for only one
  client protocol (probably DMP), in this case set
  CONFIG_SDT_SINGLE_CLIENT to 1 and define the the protocol ID of that
  client (in user_opt.h) as CONFIG_SDT_CLIENTPROTO
  
  e.g.
    #define CONFIG_SDT_SINGLE_CLIENT 1
    #define CONFIG_SDT_CLIENTPROTO DMP_PROTOCOL_ID
*/
#ifndef CONFIG_SDT_SINGLE_CLIENT
#define CONFIG_SDT_SINGLE_CLIENT 0
#endif

#if CONFIG_SDT_SINGLE_CLIENT && !defined(CONFIG_SDT_CLIENTPROTO)
#define CONFIG_SDT_CLIENTPROTO DMP_PROTOCOL_ID
#endif

/* Default SDT Mem management to follow global option */
#ifndef CONFIG_SDTMEM
#define CONFIG_SDTMEM CONFIG_MEM
#endif

/*
  Configure some limits for SDT 

  Figures here are bare minimum. You will probably want to override them.
*/
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

#endif  /* CONFIG_SDT */

/************************************************************************/
/*
  DMP
*/
/************************************************************************/
#if CONFIG_DMP

#ifndef DMP_MAX_SUBSCRIPTIONS
#define DMP_MAX_SUBSCRIPTIONS       100
#endif

/*
  DMP_HAS_VIRTUAL turns on support for virtual addressing. This is
  compliant with E1.17-2006 but virtual addressing is little used and
  the 2010 revision obsoletes it so you can probably set this false for
  significant code saving.
*/
#ifndef DMP_HAS_VIRTUAL
#define DMP_HAS_VIRTUAL       0
#endif

#endif  /* CONFIG_DMP */

/************************************************************************/
/*
  Sanity checks

  The following are sanity checks on preceding options and some
  derivative configuration values. They are not user options
*/
/************************************************************************/
/*
  check on transport selection
*/
#if (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) <= 0
#error Need to select at least one transport
#elif (CONFIG_NET_IPV4 + CONFIG_NET_IPV6) == 1
#define CONFIG_MULTI_NET 0
#else
#define CONFIG_MULTI_NET 1
#endif

/*
  checks on network stack
*/
#if (CONFIG_STACK_BSD + CONFIG_STACK_WIN32 + CONFIG_STACK_PATHWAY + CONFIG_STACK_LWIP + CONFIG_STACK_NETBURNER + CONFIG_STACK_CYGWIN) != 1
#error Need to select exactly one network stack
#endif

#if CONFIG_STACK_PATHWAY && (!CONFIG_NET_IPV4 || CONFIG_MULTI_NET)
#error Pathway stack only supports IPv4
#endif

/*
  checks for CONFIG_RLP_SINGLE_CLIENT
*/
#if ((CONFIG_SDT + CONFIG_E131) > 1 && (CONFIG_RLP_SINGLE_CLIENT) != 0)
#error Cannot support both SDT and E1.31 if CONFIG_RLP_SINGLE_CLIENT is set
#endif

#endif
