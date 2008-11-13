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
#ifndef __mcast_util_h__
#define __mcast_util_h__ 1

/************************************************************************/
#include "opt.h"

#include "component.h"
#include "netiface.h"
/*
  Prototypes
*/
#if CONFIG_EPI10
int mcast_alloc_init(ip4addr_t scopeaddr, ip4addr_t scopemask, component_t *comp);
//extern int mcast_alloc_init(ip4addr_t scopeaddr, ip4addr_t scopemask, component_t *comp);
/*
  mcast_alloc_new can be a macro
*/
#define mcast_alloc_new(comp) (scope_and_host | htonl((uint32_t)(dyn_mask & (comp)->dyn_mcast++)))

#ifdef mcast_alloc_new
extern ip4addr_t scope_and_host;	/* Network Byte Order */
extern uint16_t dyn_mask;
#else
extern groupaddr_t mcast_alloc_new(local_component_t *comp);
#endif
#endif	/* CONFIG_EPI10 */

#endif
