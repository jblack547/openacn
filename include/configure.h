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

#ifndef __configure_h__
#define __configure_h__ 1

/* underlying transport */
#define	CONFIG_NET_IPV4	1

/* define CONFIG_MULTI_NET if ACN is to work over multiple transports (e.g. IPv4 & IPv6) */
#undef CONFIG_MULTI_NET

/* socket interface */
#define	CONFIG_STACK_BSD	1
#undef	CONFIG_STACK_WATERLOO

/* memory management */
#undef CONFIG_HAS_MALLOC
#define	CONFIG_RLP_STATICMEM 1

/* EPI conformance */
#define	CONFIG_EPI10	1
#define	CONFIG_EPI11	1
#define	CONFIG_EPI12	1
#define	CONFIG_EPI13	1
#define	CONFIG_EPI15	1
#define	CONFIG_EPI16	1
#define	CONFIG_EPI17	1
#define	CONFIG_EPI18	1
#define	CONFIG_EPI19	1
#define	CONFIG_EPI20	1

#define CONFIG_SDT	1
#undef CONFIG_DMP
#undef CONFIG_E131

#define PACKED __attribute__((__packed__))

#endif
