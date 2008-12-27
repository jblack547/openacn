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
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"
#include "ntoa.h"

/*
many target stacks define ntoa as a macro using a standard function
and define HAVE_ntoa - see ntoa.h
*/
#if !defined(HAVE_ntoa)
#include <stdio.h>
#include "netxface.h"
/*
inet.h no longer defines anything useful?
#include "inet.h"
*/

/************************************************************************/
/* returns ptr to static buffer; not reentrant! */
char ip_string[16];
char * ntoa(ip4addr_t ip_addr)
{
  /* convert an ip address number into a string */
  uint8_t a,b,c,d;
  ip_addr = ntohl(ip_addr);

  a = (uint8_t)(ip_addr>>24);
  b = (uint8_t)(ip_addr>>16);
  c = (uint8_t)(ip_addr>>8);
  d = (uint8_t)(ip_addr&0xff);

  sprintf(ip_string,"%d.%d.%d.%d", a,b,c,d);

  return ip_string;
}

#endif /* !defined(HAVE_ntoa) */
