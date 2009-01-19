/*--------------------------------------------------------------------*/
/*
Copyright (c) 2008, Electronic Theatre Controls, Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Electronic Theatre Controls, Inc. nor the names of its
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

  Description:
    converts 32 bit unsigned integer to a IP address string.
*/
/************************************************************************/

#include "opt.h"
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include "aton.h"

/*
many target stacks define aton as a macro using a standard function
see aton.h
*/

#if !defined(HAVE_aton)
#include <stdlib.h>
#include "netxface.h"
/*
inet.h no longer defines anything useful?
#include "inet.h"
*/

/************************************************************************/
/*
  return ip network address from dotted notation
  Note, this very strict. it requres x.x.x.x format. where x is 1 to 3
  digits.
*/
ip4addr_t aton(const char *ip_str)
{
  /* convert an ip string to network order */
  int i;
  uint32_t ip = 0;
  const char *tmp = ip_str;

  for (i=24; ;) {
    unsigned long j;
    j=strtoul(tmp,(char **)&tmp,0);
    if (*tmp==0) {
      ip|=j;
      break;
    }
    if (*tmp=='.') {
      if (j>255) return netx_INADDR_ANY;
      ip|=(j<<i);
      if (i>0) i-=8;
      ++tmp;
      continue;
    }
    return netx_INADDR_ANY;
  }
  return htonl(ip);
}

#endif /* !defined(HAVE_aton) */
