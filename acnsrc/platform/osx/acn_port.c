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

	$Id: acn_port.c 221 2009-01-19 16:23:25Z philipnye $

*/

/*--------------------------------------------------------------------*/
#include "opt.h"
#include "acnstdtypes.h"
#include "acn_arch.h"
#include "acn_port.h"

#include <pthread.h>

pthread_mutex_t mutex;           /* Mutex */
pthread_mutexattr_t mutexattr;   /* Mutex attribute variable */
int refcnt = 0;


acn_protect_t 
acn_port_protect(void)
{
  if (!refcnt == 0) {
    /* Set the mutex as a recursive mutex */
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    /* create the mutex with the attributes set */
    pthread_mutex_init(&mutex, &mutexattr);
    /* After initializing the mutex, the thread attribute can be destroyed */
    pthread_mutexattr_destroy(&mutexattr);
  }
  /* Acquire the mutex to access the shared resource */
  pthread_mutex_lock (&mutex);
  refcnt++;
  return 0;
}

void
acn_port_unprotect(acn_protect_t param)
{
  UNUSED_ARG(param);
  if (refcnt > 1) {
    /* Unlock mutex */
    pthread_mutex_unlock (&mutex);
    refcnt--;
  } else {
    if (refcnt == 1) { 
      /* Unlock mutex */
      pthread_mutex_unlock (&mutex);
      /* Destroy / close the mutex */
      pthread_mutex_destroy (&mutex);
      refcnt--;
    }
  } /* do nothing if 0 */
}


