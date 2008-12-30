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


#include "opt.h"
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"

#include <windows.h>
#include <stdio.h>

static CRITICAL_SECTION CriticalSection;
static bool initialized_state = 0;


acn_protect_t 
acn_port_protect(void)
{
  /* printf("protect count: %d\n", protect_count); */
  if (!initialized_state) {
    /* printf("init\n"); */
    initialized_state = 1;
    InitializeCriticalSection(&CriticalSection);
  }
  /* printf("crit\n"); */
  EnterCriticalSection(&CriticalSection);
  return 0;
}

void
acn_port_unprotect(acn_protect_t param)
{
  UNUSED_ARG(param);
  /* printf("notcrit\n"); */
  LeaveCriticalSection(&CriticalSection);
  if (CriticalSection.RecursionCount == 0) {
    DeleteCriticalSection(&CriticalSection);
    initialized_state = 0;
  }
}


