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

#tabs=2s
*/
/*--------------------------------------------------------------------*/
#include "opt.h"
#include "acnstdtypes.h"
#include "acn_arch.h"
#include "acn_port.h"

#include <pthread.h>
#ifdef DEBUG_LOCKING
#include "acnlog.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#endif


pthread_mutex_t mutex;           /* Mutex */
#ifdef DEBUG_LOCKING
pthread_t owner;
#endif

void acn_port_protect_startup(void)
{
  pthread_mutexattr_t mutexattr;   /* Mutex attribute variable */

  pthread_mutexattr_init(&mutexattr);
  pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mutex, &mutexattr);
  /* After initializing the mutex, the thread attribute can be destroyed */
  pthread_mutexattr_destroy(&mutexattr);
}

void acn_port_protect_shutdown(void)
{
  pthread_mutex_destroy(&mutex);
}

#ifdef DEBUG_LOCKING
acn_protect_t acn_port_protect(void)
{
  int i = 0;
  int rslt;

  while ((rslt = pthread_mutex_trylock(&mutex)) != 0) {
    if (rslt == EBUSY) {
      if ((++i % 100) == 0) {
        acnlog(LOG_DEBUG | LOG_MISC, "Thread %d %sblocked by %d", pthread_self(), (i > 100 ? "still " : ""), owner);
      }
    } else {
        acnlog(LOG_WARNING | LOG_MISC, "Mutex error %s", strerror(rslt));
    }
    usleep(100000);
  }
  owner = pthread_self();
  return 0;
}
#endif
