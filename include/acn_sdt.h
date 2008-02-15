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
/*
This header contains declarations for the values defined in the SDT standard

Code which sits on top of SDT should not really have to use this header
*/

#ifndef __acn_sdt_h__
#define __acn_sdt_h__ 1

#include "opt.h"
#include "acn_arch.h"
#if CONFIG_EPI18
#include "epi18.h"
#endif

// TODO: Move these to opt.h
#define SDT_NUM_PACKET_BUFFERS      5   // number of past buffers we are archiving

#define RECIPROCAL_TIMEOUT_ms AD_HOC_TIMEOUT_ms * 2

#define FOREIGN_MEMBER_EXPIRY_TIME_ms  5000 // see MIN_EXPIRY_TIME_ms
#define FOREIGN_MEMBER_NAK_HOLDOFF_ms  2    // see NAK_HOLDOFF_INTERVAL_ms
#define FOREIGN_MEMBER_NAK_MODULUS     50   //
#define FOREIGN_MEMBER_NAK_MAX_TIME_ms 20   // see NAK_MAX_TIME_ms


#endif
