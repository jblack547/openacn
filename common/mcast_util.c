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

	$Id: mcast_util.c 75 2008-01-31 13:23:15Z philipnye $

*/
/*--------------------------------------------------------------------*/
#include <strings.h>
#include "opt.h"
#include "acn_arch.h"
#include "mcast_util.h"

#if CONFIG_EPI10
#include "epi10.h"

ip4addr_t scope_and_host;	/* Network Byte Order */
uint16_t dyn_mask;

/************************************************************************/
/*
  Initialize the Multicast Address allocation algorithm.
  Returns -1 for failure (invalid scopemask).
  If scopeaddr == 0 use the epi10 default address and mask
  
  All masks and addresses are passed in Network byte order
*/
int mcast_alloc_init(
	ip4addr_t scopeaddr, 
	ip4addr_t scopemask, 
	local_component_t *comp
)
{
	int HostShift;
	uint32_t HostPart;
	uint16_t uuidPart;

	// Set the scope part by masking the scope address with the scope mask.
	// If a scope argument is zero, then set the ACN EPI 10 default.
	if(scopeaddr == 0)
	{
		scopeaddr = E1_17_AUTO_SCOPE_ADDRESS;
		scopemask = E1_17_AUTO_SCOPE_MASK;
	}
	scopeaddr &= scopemask;	/* discard superfluous bits */

	// Adjust the scope mask to be within the spec range
	scopemask |= EPI10_SCOPE_MIN_MASK;
	scopemask &= EPI10_SCOPE_MAX_MASK;

	if ((scopeaddr & scopemask) != scopeaddr)
	{
		//syslog(LOG_ERR|LOG_LOCAL0,"mcast_alloc_init: Scope-address out of range.");
		return -1;
	}

/*
From epi10 r4:
	HostPart   = (IPaddress & 0xff) << HostShift
	HostShift  = 24 - ScopeBits
	ScopeBits  = bitcount(ScopeMask)

	Library function ffs (in strings.h) finds LS bit set
	with lsb numbered as 1
*/

	HostShift = ffs(ntohl(scopemask)) - 9;
	HostPart = ntohl(neti_getmyip(NULL));
	HostPart &= EPI10_HOST_PART_MASK;
	HostPart <<= HostShift;

	dyn_mask = (1 << HostShift) - 1;

	scope_and_host = scopeaddr | htonl(HostPart);

/*
*/
	uuidPart = (UUID_NODE(comp->cid)[4] << 8) | UUID_NODE(comp->cid)[5];
	uuidPart ^= (uint16_t)UUID_TIME_LOW(comp->cid);
	comp->dyn_mcast = uuidPart & dyn_mask;
	return 0;
}

/************************************************************************/
/*
  mcast_alloc_new may be defined as a macro
*/
#ifndef mcast_alloc_new
int mcast_alloc_new(local_component_t *comp)
{
	return scope_and_host | htonl((uint32_t)(dyn_mask & comp->dyn_mcast++));
}
#endif

#endif	/* CONFIG_EPI10 */
