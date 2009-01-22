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
/*
#tabs=4
*/
/*--------------------------------------------------------------------*/

#ifndef __netxface_h__
#define __netxface_h__ 1

#include <string.h>
#include "opt.h"
#include "acnstdtypes.h"

#if CONFIG_EPI20
#include "epi20.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#if (CONFIG_STACK_BSD || CONFIG_STACK_CYGWIN)
#include "netx_bsd.h"
#elif CONFIG_STACK_LWIP
#include "netx_lwip.h"
#elif CONFIG_STACK_PATHWAY
#include "netx_pathway.h"
#elif CONFIG_STACK_NETBURNER
#include "netx_netburner.h"
#elif CONFIG_STACK_WIN32
#include "netx_win32.h"
#endif

#ifdef __cplusplus
}
#endif

#endif  /* #ifndef __netxface_h__ */
