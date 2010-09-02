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
Information structures and handling relating to components
*/
#ifndef __component_h__
#define __component_h__ 1

#include "opt.h"
#include "acnstdtypes.h"

#ifndef CONFIG_COMPONENTMEM
#error CONFIG_COMPONENTMEM undefined
#endif

#include "cid.h"
#include "netxface.h"

#if CONFIG_DMP
/* #include "dmp.h" */
#endif

#if CONFIG_SDT
/* #include "sdt.h" */
#endif


/************************************************************************/
/*
  Local component structure is a rag-bag of information which represents
  a component within the local host. For efficiency, each layer can
  include it's own information within the component structure. because
  it is subject to many config options, other layers cannot rely on
  information outside their own scope and should leave it alone.

  Comonents must maintain their component structure in a fixed location
  as pointers to it may be stored and dereferenced at a later time.
*/

/* my device type */
typedef enum
{
  accUNKNOWN,
  accDEVICE,
  accCONTROL,
  accBOTH
} access_t;

#if CONFIG_SDT
typedef enum
{
  SDT_EVENT_CONNECT,
  SDT_EVENT_DISCONNECT,
  SDT_EVENT_DATA
} component_event_t;

struct component_s;

typedef void component_callback_t (
  component_event_t   state,
  struct component_s *local_component,
  struct component_s *foreign_component,
  bool                is_reliable,
  const uint8_t      *datap,
  uint32_t            data_size,
  void               *ref
);
#endif

typedef enum
{
  cbNONE,
  cbAPP,      /* created by application  */
  cbJOIN,     /* created by join request */
  cbDISC,     /* created by discover     */
  cbE131      /* created by streaming ACN */
} created_by_t;



typedef struct component_s
{
  struct component_s *next;  /* pointer to next component in linked list */
  uint16_t      usecnt;
  cid_t         cid;  /* component ID */
  bool          is_local;
  created_by_t  created_by;
#if CONFIG_EPI19
  char  fctn[ACN_FCTN_SIZE];  
#endif
#if CONFIG_EPI19 || CONFIG_E131
  char  uacn[ACN_UACN_SIZE];
#endif
#if CONFIG_EPI10
  uint16_t dyn_mcast;
#endif
#if CONFIG_SDT
  access_t   access;  /* if I am device or controller */
  netx_addr_t    adhoc_addr;
  int            adhoc_expires_at;
  struct sdt_channel_s *tx_channel;
  component_callback_t *callback; 
  bool           dirty;       /* used for discovery to tell if a component is no longer valid */
#endif
#if CONFIG_DMP
  cid_t dcid; /* discoverer CID? */
  struct dmp_subscription_s   *subscriptions;
#endif
  
} component_t;

#if CONFIG_SINGLE_COMPONENT
extern component_t the_component;
#endif

extern struct component_s *allcomps;

#if (CONFIG_COMPONENTMEM == MEM_STATIC)
extern int comp_start(void);
#else
#define comp_start() (1)
#endif

extern struct component_s *compm_new(void);
extern struct component_s *comp_listfind_by_cid(const cid_t cid, struct component_s *list);
extern struct component_s *comp_find_by_cid(const cid_t cid);
extern struct component_s *comp_get_by_cid(const cid_t cid);
extern void comp_release(struct component_s *comp);
#define comp_shutdown()

#endif
