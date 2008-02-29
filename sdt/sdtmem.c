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
This module is separated from the main SDT to allow customization of memory
handling.

*/
static const char *rcsid __attribute__ ((unused)) =
   "$Id$";

#include "opt.h"
#include "acn_arch.h"
#include "sdt.h"
#include "sdtmem.h"
#include "acnlog.h"
#include "acn_port.h"
#include "mcast_util.h"
#include "rlp.h"

#if CONFIG_SDTMEM_STATIC

static sdt_channel_t    channels_m[SDT_MAX_CHANNELS];
static sdt_member_t     members_m[SDT_MAX_MEMBERS];
static component_t  components_m[SDT_MAX_COMPONENTS];
#endif

static component_t  *components = NULL;
cid_t uuidNULL = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

#if CONFIG_SDTMEM_STATIC
/*****************************************************************************/
void
sdtm_init(void)
{
	sdt_channel_t   *channel;
  sdt_member_t    *member;
  component_t *component;
	for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; ++channel) channel->number = 0;
	for (member = members_m; member < members_m + SDT_MAX_MEMBERS; ++member) member->component = NULL;
	for (component = components_m; component < components_m + SDT_MAX_COMPONENTS; ++component) uuidNull(component->cid);
}    

/*****************************************************************************/
/*
  Create a new channel
*/
sdt_channel_t *
sdtm_add_channel(component_t *leader, uint16_t channel_number, bool is_local)
{
	sdt_channel_t *channel;
  sys_prot_t     protect;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_channel %d", channel_number);

  /* can't have channel number 0 */
  if (channel_number == 0) {
    return NULL; 
  }

  //TODO: should we check to see if this exists already?

  /* find and empty one */
  for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; channel++) {
		if (channel->number == 0) {
      protect = acn_port_protect();
      /* clear values */
      memset(channel, 0, sizeof(sdt_channel_t));
      /* assign this to our leader */
      leader->tx_channel = channel;
      /* assign passed and default values */
      channel->number  = channel_number;
      channel->available_mid     = 1;
      channel->is_local          = is_local;
      channel->total_seq         = 1;
      channel->reliable_seq      = 1;
      channel->oldest_avail      = 1;
      //channel->member_list     = // added when creating members
      //channel->sock;           = // default null
      //channel->listener;       = // multicast listener, default null
      acn_port_unprotect(protect);

      return channel;
    }
  }
  acnlog(LOG_ERR | LOG_SDTM,"sdtm_add_channel: none left");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused channel
*/
void 
sdtm_remove_channel(component_t *leader)
{
  sys_prot_t    protect;
  sdt_member_t  *member;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_channel: %d", leader->tx_channel->number);

  /* remove members */
  if (leader->tx_channel) {
    member = leader->tx_channel->member_list;
    while(member) {
      member = sdtm_remove_member(leader->tx_channel, member);
    }
    /* remove listener */
    if (leader->tx_channel->listener) {
      rlp_del_listener(leader->tx_channel->sock, leader->tx_channel->listener);
    }
  }

  /* channel->sock is freed outside of this scope */

  /* now mark it empty */
  protect = acn_port_protect();
  /* mark channel as unused */
  leader->tx_channel->number = 0; /* mark it empty */
  /* remove it from the leader */
  leader->tx_channel = NULL;
  acn_port_unprotect(protect);
  return;
}


/*****************************************************************************/
/*
Add member to channel
*/
sdt_member_t *
sdtm_add_member(sdt_channel_t *channel, component_t *component)
{
  sdt_member_t *member;
  sys_prot_t   protect;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_member to channel %d", channel->number);

  // TODO: should we verify the component does not alread exist?
  /* find and empty one */
  for (member = members_m; member < members_m + SDT_MAX_MEMBERS; member++) {
		if (member->component == NULL) {
      /* clear values */
      protect = acn_port_protect();
      memset(member, 0, sizeof(sdt_member_t));
      /* put this one at the head of the list */
      member->next = channel->member_list;
      channel->member_list = member;
      /* assign passed and default values */
      member->component     = component;
      //member->mid           = // default 0
      //member->nak           = // default 0
      //member->state         = // deafult 0 = msEMPTY;
      //member->expiry_time_s = // default 0
      member->expires_ms   = -1;// expired
      //member->mak_ms      = // default 0

      /* only used for local member */
      //member->nak_holdoff  = // default 0
      //member->last_acked   = // default 0
      //member->nak_modulus  = // default 0
      //member->nak_max_wait  = // default 0
      acn_port_unprotect(protect);
      return member;
    }
  }
  acnlog(LOG_ERR | LOG_SDTM,"sdtm_add_member: none left");

  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Remove member

    Return next member or null
*/
sdt_member_t *
sdtm_remove_member(sdt_channel_t *channel, sdt_member_t *member)
{
  sdt_member_t *cur;
  sys_prot_t   protect;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_member: %d from channel %d", member->mid, channel->number);

  /* if it is at top, then we just move leader */
  if (member == channel->member_list) {
    protect = acn_port_protect();
    channel->member_list = member->next;
    member->component = NULL; /* mark it empty */
    acn_port_unprotect(protect);
    return channel->member_list;
  }
  
  /* else run through list in order, finding previous one */
  cur = channel->member_list;
  while (cur) {
    if (cur->next == member) {
      protect = acn_port_protect();
      /* jump around it */
      cur->next = member->next;
      member->component = NULL; /* mark it empty */
      acn_port_unprotect(protect);
      return cur->next;
    }
    cur = cur->next;
  }
  return NULL;
}

/*****************************************************************************/
/*
  
*/
component_t *
sdtm_add_component(const cid_t cid, const cid_t dcid, bool is_local)
{
  component_t *component;
  sys_prot_t   protect;

  #if LOG_DEBUG | LOG_SDTM
  char  uuid_text[37];
  uuidToText(cid, uuid_text);
  #endif

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_component: %s", uuid_text);

  /* find and empty one */
  for (component = components_m; component < components_m + SDT_MAX_COMPONENTS; component++) {
		if (uuidIsNull(component->cid)) {
      /* clear values */
      protect = acn_port_protect();
      memset(component, 0, sizeof(component_t));
      /* put this one at the head of the list */
      component->next = components;
      components = component;
      /* assign values */
      if (cid) 
        uuidCopy(component->cid, cid);
      if (dcid)
        uuidCopy(component->dcid, dcid);
      //component->adhoc_expires_at = // default 0
      component->is_local = is_local;
      //tx_channel = 0 // default
      acn_port_unprotect(protect);
      if (is_local) {
        mcast_alloc_init(0, 0, component);
      }
      //is_receprocal     // default 0 (false)
      return component;
    }
  }
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_component: none left");


  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Remove component
*/
component_t *
sdtm_remove_component(component_t *component)
{
  component_t *cur;
  sys_prot_t   protect;

  #if LOG_DEBUG | LOG_SDTM
  char  uuid_text[37];
  uuidToText(component->cid, uuid_text);
  #endif

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_component: %s", uuid_text);

  /* remove our channel (and member list) */
  if (component->tx_channel) {
    sdtm_remove_channel(component);
  }

  /* if it is at top, then we just move leader */
  if (component == components) {
    protect = acn_port_protect();
    components = component->next;
    uuidNull(component->cid); /* mark it empty */
    acn_port_unprotect(protect);
    return components;
  }
  
  /* else run through list in order, finding previous one */
  cur = components;
  while (cur) {
    if (cur->next == component) {
      protect = acn_port_protect();
      /* jump around it */
      cur->next = component->next;
      uuidNull(component->cid); /* mark it empty */
      acn_port_unprotect(protect);
      return (cur->next);
    }
    cur = cur->next;
  }
  return NULL;
}
#endif

#if CONFIG_SDTMEM_MALLOC
/*****************************************************************************/
void
sdtm_init(void)
{
   
}    

/*****************************************************************************/
/*
  Create a new channel
*/
sdt_channel_t *
sdtm_add_channel(component_t *leader, uint16_t channel_number, bool is_local)
{
    sdt_channel_t *channel;

    acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_channel %d", channel_number);

  /* can't have channel number 0 */
    if (channel_number == 0) return NULL; 

    channel = malloc(sizeof(sdt_channel_t));
    if(!channel){
        acnlog(LOG_ERR | LOG_SDTM,"sdtm cannot add channel: Out of Memory");
        return NULL;
    }
    memset(channel,0,sizeof(sdt_channel_t));  
    
    /* assign this to our leader */
    leader->tx_channel = channel;
    /* assign passed and default values */
    channel->number  = channel_number;
    channel->available_mid     = 1;
    channel->is_local          = is_local;
    channel->total_seq         = 1;
    channel->reliable_seq      = 1;
    channel->oldest_avail      = 1;
    //channel->member_list     = // added when creating members
    //channel->sock;           = // default null
    //channel->listener;       = // multicast listener, default null
    return channel;
}
  
/*****************************************************************************/
/*
  Free an unused channel
*/
void 
sdtm_remove_channel(component_t *leader)
{
  sdt_member_t  member;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_channel: %d", leader->tx_channel->number);
  
  /* remove members */
  if (leader->tx_channel) {
    member = leader->tx_channel->member_list;
    while(member) {
      member = sdtm_remove_member(leader->tx_channel, member);
    }
    /* remove listener */
    if (leader->tx_channel->listener) {
      rlp_del_listener(leader->tx_channel->sock, leader->tx_channel->listener);
    }
  }

  /* channel->sock is freed outside of this scope */

  /* remove it from the leader */
  free(leader->tx_channel);
  leader->tx_channel = NULL;
  return;
}
  
/*****************************************************************************/
/*
    Add member to channel
*/
sdt_member_t *
sdtm_add_member(sdt_channel_t *channel, component_t *component)
{
    sdt_member_t *member;

    acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_member to channel %d", channel->number);

    // TODO: should we verify the component does not already exist?
    member = malloc(sizeof(sdt_member_t));
    
    if(!member){
        acnlog(LOG_ERR | LOG_SDTM,"sdtm_add_member: Out of memory.");
        return NULL;
    }
    memset(member,0,sizeof(sdt_member_t));
    
    member->next = channel->member_list;
    channel->member_list = member;
    /* assign passed and default values */
    member->component     = component;
    //member->mid         = // default 0
    //member->nak         = // default 0
    //member->state         = // deafult 0 = msEMPTY;
    //member->expiry_time_s = // default 0
    member->expires_ms   = -1;// expired
    //member->mak_ms      = // default 0
    
    /* only used for local member */
    //member->nak_holdoff  = // default 0
    //member->last_acked   = // default 0
    //member->nak_modulus  = // default 0
    //member->nak_max_wait  = // default 0
    return member;
}


/*****************************************************************************/
/*
  Remove member

  Return next member or null
*/
sdt_member_t *
sdtm_remove_member(sdt_channel_t *channel, sdt_member_t *member)
{
  sdt_member_t *cur;


  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_member: %d from channel %d", member->mid, channel->number);
  
  /* if it is at top, then we just move leader */
  if (member == channel->member_list) {
    channel->member_list = member->next;
    free(member);
    return channel->member_list;
  }
  
  /* else run through list in order, finding previous one */
  cur = channel->member_list;
  while (cur) {
    if (cur->next == member) {
      /* jump around it */
      cur->next = member->next;
      free(member);
      return cur->next;
    }
    cur = cur->next;
  }
  return NULL;
}

/*****************************************************************************/
/*
   Add a new component
*/
component_t *
sdtm_add_component(const cid_t cid, const cid_t dcid, bool is_local)
{
    component_t *component;
    
    #if LOG_DEBUG | LOG_SDTM
    char  uuid_text[37];
    uuidToText(cid, uuid_text);
    #endif
  
    acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_add_component: %s", uuid_text);
    
    component = malloc(sizeof(component_t));
    if(!component){
        acnlog(LOG_ERR | LOG_SDTM,"sdtm_add_component: Out of memory.");
        return NULL;
    }
    memset(component, 0, sizeof(component_t));
    /* put this one at the head of the list */
    component->next = components;
    components = component;
    /* assign values */
    if (cid) 
      uuidCopy(component->cid, cid);
    if (dcid)
      uuidCopy(component->dcid, dcid);
    //component->adhoc_expires_at = // default 0
    component->is_local = is_local;
    if (is_local) {
      mcast_alloc_init(0, 0, component);
    }
    return component;
}

/*****************************************************************************/
/*
  Remove component
*/
component_t *
sdtm_remove_component(component_t *component)
{
  component_t *cur;


  #if LOG_DEBUG | LOG_SDTM
  char  uuid_text[37];
  uuidToText(component->cid, uuid_text);
  #endif

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_remove_component: %s", uuid_text);

  /* remove our channel (and member list) */
  if (component->tx_channel) {
    sdtm_remove_channel(component);
  }

  /* if it is at top, then we just move leader */
  if (component == components) {
    components = component->next;
    free(component);
    return components;
  }
  
  /* else run through list in order, finding previous one */
  cur = components;
  while (cur) {
    if (cur->next == component) {
      /* jump around it */
      cur->next = component->next;
      free(component);
      return (cur->next);
    }
    cur = cur->next;
  }
  return NULL;
}

#endif /* CONFIG_SDTMEM_MALLOC */

/******************************************************************************** 
    Functions that do not depend on a particular memory model continue from here
*/

/*****************************************************************************/
/*
  find next available MID for a channel
*/
uint16_t 
sdtm_next_member(sdt_channel_t *channel)
{
    if (channel->available_mid == 0x7FFF)
      channel->available_mid = 1;

    /* find MID typically it will the next MID */
    /* we are going to assume that we will always find one! */
    while(sdtm_find_member_by_mid(channel, channel->available_mid)) 
      channel->available_mid++;

    /* return this one and go ahead in increment to next */
    return channel->available_mid++;
}

/*****************************************************************************/
/*
  Find member by MID
*/
sdt_member_t *
sdtm_find_member_by_mid(sdt_channel_t *channel, uint16_t mid)
{
  sdt_member_t *member;
  
  member = channel->member_list;
  
  while(member) {
    if (member->mid == mid) {
      return member;
    }
    member = member->next;
  }
  return NULL;  /* oops */
}

/*****************************************************************************/
/*
  Find member by Component
*/
sdt_member_t *
sdtm_find_member_by_component(sdt_channel_t *channel, component_t *component)
{
  sdt_member_t *member;
  
  member = channel->member_list;
  
  while(member) {
    if (member->component == component) {
      return member;
    }
    member = member->next;
  }
  return NULL;  /* oops */
}

/*****************************************************************************/
/*
  
*/
component_t *
sdtm_first_component(void)
{
  return components;
}

/*****************************************************************************/
/*
  
*/
component_t *
sdtm_find_component(const cid_t cid)
{
  component_t *component;
  
  component = components;
  
  while(component) {
    if (uuidIsEqual(component->cid, cid)) {
      return (component);
    }
    component = component->next;
  }
  return NULL;  /* oops */
}

