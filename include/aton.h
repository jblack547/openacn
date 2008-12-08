/*--------------------------------------------------------------------*/
/*
Copyright (c) 2008, Electronic Theatre Controls, Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Electronic Theatre Controls, Inc. nor the names of its
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
  
  Description:
    Header file for aton.c
*/
#ifndef __ATON_H__
#define __ATON_H__

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/*
many target stacks already provide an aton function so we just wrap that
up as a macro. A simple version is provided for the few which don't
*/
#if CONFIG_STACK_WIN32

#include <winsock2.h>
#define aton(ip_str) inet_addr(ip_str)
#define HAVE_aton 1

#elif CONFIG_STACK_BSD

/*
#include <sys/socket.h>
#include <netinet/in.h>
*/
#include <arpa/inet.h>
#define aton(ip_str) inet_addr(ip_str)
#define HAVE_aton 1

#elif CONFIG_STACK_NETBURNER

/*
FIXME: Should this be htonl(AsciiToIp(ip_str)) ?

netburner (m68k) is big endian so it will be the same, but which other
architectures is the same stack available for?
*/
#include "utils.h"
#define aton(ip_str) AsciiToIp(ip_str);
#define HAVE_aton 1

#else
#include "netxface.h"
ip4addr_t aton(const char *ip_str);

#endif

#ifdef __cplusplus
}
#endif

#endif //__ATON_H__
