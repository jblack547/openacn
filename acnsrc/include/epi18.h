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

#tabs=3s
*/
/*--------------------------------------------------------------------*/

#ifndef __epi18_h__
#define __epi18_h__ 1

#if ACN_VERSION == 20060000

#define MAK_TIMEOUT_ms           200
#define MAK_MAX_RETRIES          3
#define AD_HOC_TIMEOUT_ms        200
#define AD_HOC_RETRIES           3
#define MIN_EXPIRY_TIME_ms       2000
#define NAK_TIMEOUT_ms           100
#define NAK_MAX_RETRIES          3
#define NAK_HOLDOFF_INTERVAL_ms  2
#define NAK_MAX_TIME_ms          (10 * NAK_HOLDOFF_INTERVAL_ms)
#define NAK_BLANKTIME_ms         (3 * NAK_HOLDOFF_INTERVAL_ms)
#define SDT_MULTICAST_PORT       5568

#elif ACN_VERSION == 20100000

#define MAK_TIMEOUT_FACTOR          0.1
#define MAK_MAX_RETRIES             2        /* 3 tries total */
#define AD_HOC_TIMEOUT_ms           200      /* ms */
#define AD_HOC_RETRIES              2        /* 3 tries total */
#define RECIPROCAL_TIMEOUT_FACTOR   0.2
#define MIN_EXPIRY_TIME_s           2        /* s */
#define NAK_TIMEOUT_FACTOR          0.1
#define NAK_MAX_RETRIES             2        /* 3 tries total */
#define NAK_HOLDOFF_INTERVAL_ms     2        /* ms */
#define NAK_MAX_TIME(hldoff)        (10 * (hldoff))     /* x NAK_HOLDOFF_INTERVAL */
#define NAK_BLANKTIME(hldoff)       (3  * (hldoff))     /* x NAK_HOLDOFF_INTERVAL */
#define NAK_MAX_TIME_ms  NAK_MAX_TIME(NAK_HOLDOFF_INTERVAL_ms)
#define NAK_BLANKTIME_ms NAK_BLANKTIME(NAK_HOLDOFF_INTERVAL_ms)
#define SDT_MULTICAST_PORT          5568     /* IANA registered port "sdt" */

#else
#error Unknown ACN version
#endif

#endif
