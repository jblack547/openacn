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

*/
/*--------------------------------------------------------------------*/
#include "opt.h"
#include "types.h"
#include "acn_arch.h"

/* #if CONFIG_STACK_NETBURNER */
/* #include "includes.h"      */ /* netburner types */
/* #include "have_types.h" */
/* #include <ucos.h> */
/* #endif */

#if CONFIG_STACK_LWIP
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#endif  /* CONFIG_STACK_LWIP */

#include "acn_arch.h"
#include "dmpmem.h"
#include "component.h"
#include "acnlog.h"
 
 
/* holder... */
dmp_subscription_t subscriptions_m[DMP_MAX_SUBSCRIPTIONS];

#ifdef USE_QUEUE
#define MAX_DMP_QUEUE 24

/* local variables */
/* static dmp_queue_t *input_ptr;        /* fifo input pointer*/ */
/* static dmp_queue_t *output_ptr;       /* fifo output pointer */ */
dmp_queue_t dmp_queue[MAX_DMP_QUEUE]; /* fifo */
#endif

#if CONFIG_STACK_NETBURNER
OS_SEM dmp_queue_sem;             /* fifo semaphore */
#endif

#if CONFIG_STACK_LWIP
sys_sem_t dmp_queue_sem = 0;      /* fifo semaphore */
#endif

#ifdef USE_QUEUE
#define DMP_LOCK_QUEUE()   sys_sem_wait(dmp_queue_sem)
#define DMP_UNLOCK_QUEUE() sys_sem_signal(dmp_queue_sem)
#endif

/*****************************************************************************/
/* init routine 
*/
void
dmpm_init(void)
{
  dmp_subscription_t   *subscription;

  for (subscription = subscriptions_m; subscription < subscriptions_m + DMP_MAX_SUBSCRIPTIONS; ++subscription) subscription->state = DMP_SUB_EMPTY;

#ifdef USE_QUEUE
  /* get ourselves a semaphore */
  dmp_queue_sem = sys_sem_new(1);
  input_ptr = &dmp_queue[0];
  output_ptr = input_ptr;
#endif /* USE_QUEUE */


}


/*****************************************************************************/
/* shut it down 
*/
void
dmpm_close(void)
{
#ifdef USE_QUEUE
  /* close any open pbufs */
  while (dmpm_get_queue());

  /* free our semaphore */
  if (dmp_queue_sem) {
    sys_sem_free(dmp_queue_sem);
  }
#endif /* USE_QUEUE */
}

#ifdef USE_QUEUE
/*****************************************************************************/
/*
  Put mail in our mailbox

  Simple FIFO using zero copy  
  it is designed so one thread can input records and the other
  can remove them.
*/
void 
dmpm_add_queue(component_t *local_component, component_t *foreign_component, bool is_reliable, const uint8_t *data, uint32_t data_len, void *ref)
{
  dmp_queue_t *next_ptr;
  DMP_LOCK_QUEUE();
  /* make sure we don't overrun */
  if (input_ptr == &dmp_queue[MAX_DMP_QUEUE-1]) {
    next_ptr = &dmp_queue[0];
  } else {
    next_ptr = input_ptr + 1;
  }
  /* make sure we don't overlap output */
  if (next_ptr == output_ptr) {
    /* log error here! */
    DMP_UNLOCK_QUEUE();
    return;
  } else {
    input_ptr = next_ptr;
  }
  acnlog(LOG_INFO | LOG_DMPM,"dmpm_add_queue: %d", input_ptr - &dmp_queue[0]);
  input_ptr->local_component = local_component;
  input_ptr->foreign_component = foreign_component;
  input_ptr->is_reliable = is_reliable;
  input_ptr->data = data;
  input_ptr->data_len = data_len;
  input_ptr->ref = ref;
  /* increment our reference pointer */
  pbuf_ref(ref);
  DMP_UNLOCK_QUEUE();
}

/*****************************************************************************/
/* 
  Get mail from our mailbox - if any 
*/
dmp_queue_t *
dmpm_get_queue(void)
{
  dmp_queue_t *rtn_ptr;

  /* nuke the current one just so free_queue does not have be called each time */
  dmpm_free_queue();
  /* is there anything in the queue? */
  DMP_LOCK_QUEUE();
  if (input_ptr != output_ptr) {
    /* get next queue */
    if (output_ptr == &dmp_queue[MAX_DMP_QUEUE-1]) {
      output_ptr = &dmp_queue[0];
    } else {
      output_ptr = output_ptr + 1;
    }
    acnlog(LOG_INFO | LOG_DMPM,"dmpm_get_queue: got_queue: %d",  output_ptr - &dmp_queue[0]);
    rtn_ptr = output_ptr;
  } else {
    rtn_ptr = NULL;
  }
  DMP_UNLOCK_QUEUE();
  return rtn_ptr; 
}

/*****************************************************************************/
/* 
  Free queue memory as pointed to by output pointer
*/
void
dmpm_free_queue(void) 
{
  DMP_LOCK_QUEUE();
  if (output_ptr->local_component) {
    acnlog(LOG_INFO | LOG_DMPM,"dmpm_free_queue: queue freed: %d",  output_ptr - &dmp_queue[0]);
    /* decrement our reference pointer */
    pbuf_free(output_ptr->ref);
    /* mark it free locally */
    output_ptr->local_component = 0;
  }
  DMP_UNLOCK_QUEUE();
}
#endif  /* USE_QUEUE */

/*****************************************************************************/
/*
  allocate a new subscription
*/
dmp_subscription_t *
dmpm_new_subscription(void)
{ 
  /* allocate */
  dmp_subscription_t   *subscription;

  for (subscription = subscriptions_m; subscription < subscriptions_m + DMP_MAX_SUBSCRIPTIONS; ++subscription) {
    if (subscription->state == DMP_SUB_EMPTY) {
      /* clear values */
      memset(subscription, 0, sizeof(dmp_subscription_t));
      return subscription;
    }
  }
  /* oops out of resources  */
  acnlog(LOG_WARNING | LOG_DMPM,"dmpm_new_subscription: out of subscriptions");
  return NULL;
}

/*****************************************************************************/
/*
  free unused suscription
*/
void 
dmpm_free_subscription(dmp_subscription_t *subscription)
{
  subscription->state = DMP_SUB_EMPTY;
  return;
}



