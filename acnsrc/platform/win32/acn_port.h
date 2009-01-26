/*--------------------------------------------------------------------*/
/*

Copyright (c) 2008, Electronic Theatre Controls, Inc

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

#ifndef __acn_port_h__
#define __acn_port_h__ 1

#include "opt.h"

#if CONFIG_STACK_WIN32
/* using winsock 2 */
#include <winsock2.h>
#include <ws2tcpip.h>

/* WIN32 has it's own version of these */
#define HAVE_htons
#define HAVE_htonl
#define HAVE_ntohs
#define HAVE_ntohl
#include "byteorder.h"

/* ignore warnings: */
/* This function or variable may be unsafe. Consider using xxx instead.
   To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details. */
/* #define _CRT_SECURE_NO_WARNINGS <- does not seem to work*/
#pragma warning(disable:4996)

/* msvsc++ uses a different prefix */
#define __func__ __FUNCTION__

/* Defined platform specific sprintf() */
#define SPRINTF(buf, ...) sprintf(buf, __VA_ARGS__)
#define PRINTF(format, ...) printf(format, __VA_ARGS__)

/* platform specific means to lock resources */
typedef int acn_protect_t;
#define ACN_PORT_PROTECT()        0; EnterCriticalSection(&CriticalSection);
#define ACN_PORT_UNPROTECT(x)     LeaveCriticalSection(&CriticalSection);


#ifdef __cplusplus
extern "C" {
#endif

extern CRITICAL_SECTION CriticalSection;

/* prevent other threads */

/* allow other threads */
void acn_port_protect_startup(void);
void acn_port_protect_shutdown(void);


#ifdef __cplusplus
}
#endif


#endif /* CONFIG_STACK_WIN32 */

#endif
