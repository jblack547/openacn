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

#ifndef __sdtmem_h__
#define __sdtmem_h__ 1

#include "opt.h"
#include "sdt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_SDTMEM == MEM_STATIC
extern void sdtm_init(void);

extern sdt_channel_t *sdtm_new_channel(void);
extern void           sdtm_free_channel(sdt_channel_t *channel);

extern sdt_member_t  *sdtm_new_member(void);
extern void           sdtm_free_member(sdt_member_t *member);

extern sdt_resend_t  *sdtm_new_resend(void);
extern void           sdtm_free_resend(sdt_resend_t *resend);
extern void           sdtm_free_resends(void);

#elif CONFIG_SDTMEM == MEM_MALLOC
#include <stdlib.h>

#define sdtm_init()

#define sdtm_allocblank(type) ((type *)calloc(1, sizeof(type)))

#define sdtm_new_channel()      sdtm_allocblank(sdt_channel_t)
#define sdtm_new_member()       sdtm_allocblank(sdt_member_t)
#define sdtm_new_resend()       sdtm_allocblank(sdt_resend_t)

#define sdtm_free_channel(x) free(x)
#define sdtm_free_member(x) free(x)
#define sdtm_free_resend(x) free(x)

extern void sdtm_free_resends(void);

#endif /* CONFIG_SDTMEM == MEM_MALLOC */

#ifdef __cplusplus
}
#endif

#endif
