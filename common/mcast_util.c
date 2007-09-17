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

	$Id: mcast_util.c 11 2007-09-07 09:02:15Z philip $

*/
/*--------------------------------------------------------------------*/
#include "uuid.h"
#include "acn_config.h"
#include <string.h>
#include "wattcp.h"

// ACN EPI 10 Constants
static const uint32_t E1_17_AUTO_SCOPE_ADDRESS = 0xEFC00000;    // 239.192.0.0
static const uint32_t E1_17_AUTO_SCOPE_MASK = 0xFFFC0000;       // 255.252.0.0
static const int HOST_PART_BITS = 8;        // # of bits in the host port field
static const uint32_t HOST_PART_MASK = 0xFF;
static const uint32_t  SCOPE_PART_MIN_MASK = 0xFFC00000;  // Min allowed scope part
static const uint32_t SCOPE_PART_MAX_MASK = 0xFFFF0000;  // Max allowed scope part

static uint32_t dyn_mask = 0;
static int scope_mask_num_bits = 0;
static int host_part_shift = 0;
static uint32_t scope_address = 0;
static uint32_t scope_mask = 0;

void mcast_alloc_init(int scopeaddr, int scopemask, local_component_t *comp)
{
	// Set the scope part by masking the scope address with the scope mask.
	// If a scope argument is zero, then set the ACN EPI 10 default.
	if(scopeaddr == 0)
		scopeaddr = E1_17_AUTO_SCOPE_ADDRESS;
	if(scopemask == 0)
		scopemask = E1_17_AUTO_SCOPE_MASK;
	// Adjust the scope mask to be within the spec range
	scopemask |= SCOPE_PART_MIN_MASK;
	scopemask &= SCOPE_PART_MAX_MASK;

	scope_address = scopeaddr;
	scope_mask = scopemask;
	// Determine location of LSBit in scope mask and set host part shift value.
	scope_mask_num_bits = 32 - ffs(scopemask) + 1;
	host_part_shift = 32 - scope_mask_num_bits - HOST_PART_BITS;
	// Host part mask is a constant, fixed by spec.

	// Set dynamic part mask according to location of scope and host parts.
	// The host part has a fixed size, 8 bits.
	// dyna_part_mask now has one bit at the location of the LSBit for the scope part.
	// Shift the mask bit down to the LSBit of the host part, and
	// subtract one to turn it into a mask for the bits of the dynamic part.
	
	int dyn_bits = host_part_shift;
	
	while (dyn_bits)
	{
		dyn_mask <<= 1;
		dyn_mask |= 1;
		dyn_bits--;
	}
	
	uuid *uuid = (void*)comp->cid;
	comp->dyn_mcast = (((uint32_t)uuid->time_low) ^ ((uint32_t)uuid->node[2])) & dyn_mask;
}

int mcast_alloc_new(local_component_t * comp)
{
	comp->dyn_mcast = (comp->dyn_mcast + 1) & dyn_mask;
	return (scope_address & scope_mask) | ((my_ip_addr & HOST_PART_MASK) << host_part_shift) | comp->dyn_mcast;
}
