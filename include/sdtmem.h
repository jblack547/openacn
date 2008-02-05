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

	$Id: sdtmem.h  $

*/
/*--------------------------------------------------------------------*/

#ifndef __sdtmem_h__
#define __sdtmem_h__ 1

#include "opt.h"
#include "acn_arch.h"

extern void sdtm_init(void);

extern sdt_channel_t *sdtm_add_channel(component_t *leader, uint16_t channel_number, bool is_local);
extern void           sdtm_remove_channel(component_t *leader);

extern sdt_member_t  *sdtm_find_member_by_mid(sdt_channel_t *channel, uint16_t mid);
extern sdt_member_t  *sdtm_find_member_by_component(sdt_channel_t *channel, component_t *component);
extern sdt_member_t  *sdtm_add_member(sdt_channel_t *channel, component_t *component);
extern sdt_member_t  *sdtm_remove_member(sdt_channel_t *channel, sdt_member_t *member);
extern uint16_t       sdtm_next_member(sdt_channel_t *channel);


extern component_t *sdtm_find_component(cid_t cid);
extern component_t *sdtm_add_component(cid_t cid, cid_t dcid, bool is_local);
extern component_t *sdtm_first_component(void);

extern component_t *sdtm_remove_component(component_t *component);

#endif
