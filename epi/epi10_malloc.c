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

	$Id$

*/
/*--------------------------------------------------------------------*/
#include <strings.h>
#include "opt.h"
#include "acn_arch.h"
#include "netiface.h"

#if CONFIG_EPI10
#include "epi10.h"

static uint32_t scope_and_host;
static uint32_t dyn_mask = 0x3ff;

int mcast_alloc_init(
	int scopeaddr, 
	int scopemask, 
	local_component_t *comp
)
{
	int HostShift;
	uint32_t HostPart;
	struct uuidStruct_s *uuidp;
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
		syslog(LOG_ERR|LOG_LOCAL0,"mcast_alloc_init: Scope-address out of range.");
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

	HostShift = ffs(scopemask) - 9;
	HostPart = (neti_getmyip(NULL) & EPI10_HOST_PART_MASK) << HostShift;

	dyn_mask = (1 << HostShift) - 1;
	scope_and_host = scopeaddr | HostPart;

/*
	Set dynamic part mask according to location of scope and host parts.
	The host part has a fixed size, 8 bits.
	dyna_part_mask now has one bit at the location of the LSBit for the scope part.
	Shift the mask bit down to the LSBit of the host part, and
	subtract one to turn it into a mask for the bits of the dynamic part.
*/
	uuidp = (struct uuidStruct_s *)comp->cid;
	uuidPart = *(uint16_t *)(uuid->node + 4);
	uuidPart ^= (uint16_t)uuid->time_low;
	comp->dyn_mcast = uuidPart & dyn_mask;
}

int mcast_alloc_new(local_component_t *comp)
{
	return scope_and_host | (dyn_mask & comp->dyn_mcast++);
}
#endif
