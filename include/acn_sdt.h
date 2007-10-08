/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Pathway Connectivity Inc.

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

	$Id: sdt.h 2 2007-09-17 09:31:30Z philipnye $

*/
/*--------------------------------------------------------------------*/
/*
This header contains declarations for the values defined in the SDT standard

Code which sits on top of SDT should not really have to use this header
*/

#ifndef __acn_sdt_h__
#define __acn_sdt_h__

#include <arch/types.h>

/* ESTA registered protocol code */
#define PROTO_SDT  1

/* PDU vector codes [SDT spec Table 3] */
enum
{
	SDT_REL_WRAPPER		= 1,
	SDT_UNREL_WRAPPER	= 2,
	SDT_CHANNEL_PARAMS	= 3,
	SDT_JOIN			= 4,
	SDT_JOIN_REFUSE		= 5,
	SDT_JOIN_ACCEPT		= 6,
	SDT_LEAVE			= 7,
	SDT_LEAVING			= 8,
	SDT_CONNECT			= 9,
	SDT_CONNECT_ACCEPT	= 10,
	SDT_CONNECT_REFUSE	= 11,
	SDT_DISCONNECT		= 12,
	SDT_DISCONNECTING	= 13,
	SDT_ACK				= 14,
	SDT_NAK				= 15,
	SDT_GET_SESSIONS	= 16,
	SDT_SESSIONS		= 17, 
};

/* Reason codes [SDT spec Table 6] */
enum 
{
	SDT_REASON_NONSPEC			= 1,
	SDT_REASON_PARAMETERS		= 2,
	SDT_REASON_RESOURCES		= 3,
	SDT_REASON_ALREADY_MEMBER	= 4,
	SDT_REASON_BAD_ADDR			= 5,
	SDT_REASON_NO_RECIPROCAL	= 6,
	SDT_REASON_CHANNEL_EXPIRED	= 7,
	SDT_REASON_LOST_SEQUENCE	= 8,
	SDT_REASON_SATURATED		= 9,
	SDT_REASON_LEAVE_REQUESTED	= 10,
};

/* Address specification types [SDT spec Table 7] */
enum
{
	SDT_ADDR_NULL = 0,
	SDT_ADDR_IPV4 = 1,
	SDT_ADDR_IPV6 = 2,
};

/* transport layer addresses */
#if defined(CONFIG_MULTI_NET)
#error Multiple networks not yet supported
#else

#if defined(CONFIG_NET_IPV4)
struct PACKED transportLayerAddress_s {
	uint8_t type;	//must be SDT_ADDR_IPV4
	uint16_t n_port;
	netAddr_t n_addr;
};

#elif defined(CONFIG_NET_IPV6)
struct PACKED transportLayerAddress_s {
	uint8_t type;	//must be SDT_ADDR_IPV6
	uint16_t n_port;
	netAddr_t n_addr;
};

#endif
#endif


#if defined(CONFIG_EPI18)
#include "epi18.h"
#endif

#endif
