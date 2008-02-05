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

	$Id: sdtmem.c $

*/
/*--------------------------------------------------------------------*/
/*
This module is separated from the main SDT to allow customization of memory
handling.

*/
static const char *rcsid __attribute__ ((unused)) =
   "$Id: sdtmem.c $";

#include "opt.h"
#include "acn_arch.h"
#include "sdt.h"
#include "sdtmem.h"

#if CONFIG_SDTMEM_STATIC
static component_t  *components = NULL;

static sdt_channel_t    channels_m[SDT_MAX_CHANNELS];
static sdt_member_t     members_m[SDT_MAX_MEMBERS];
static component_t  components_m[SDT_MAX_COMPONENTS];
#endif

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

  /* can't have channel number 0 */
  if (channel_number == 0) return NULL; 

  //TODO: should we check to see if this exists already?

  /* find and empty one */
  for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; channel++) {
		if (channel->number == 0) {
      /* clear values */
      memset(channel, 0, sizeof(sdt_channel_t));
      /* assign this to our leader */
      leader->tx_channel = channel;
      /* assign passed and default values */
      channel->number  = channel_number;
      channel->available_mid     = 1;
      //channel->pending        = // default 0
      //channel->state          = // default 0
      //channel->downstream.port = // default 0
      //channel->downstream.ip   = // default 0
      channel->is_local          = is_local;
      channel->total_seq         = 1;
      channel->reliable_seq      = 1;
      channel->oldest_avail      = 1;
      //channel->member_list     = // added when creating members
      return channel;
    }
  }
  printf("sdtm_add_channel: none left\n");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused channel
*/
void 
sdtm_remove_channel(component_t *leader)
{
  /* mark channel as unused */
  leader->tx_channel->number = 0; /* mark it empty */
  /* remove it from the leader */
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

  // TODO: should we verify the component does not alread exist?
  /* find and empty one */
  for (member = members_m; member < members_m + SDT_MAX_MEMBERS; member++) {
		if (member->component == NULL) {
      /* clear values */
      memset(member, 0, sizeof(sdt_member_t));
      /* put this one at the head of the list */
      member->next = channel->member_list;
      channel->member_list = member;
      /* assign passed and default values */
      member->component     = component;
      //member->mid         = // default 0
      //member->nak         = // default 0
      //member->pending     = // default 0
      //member->is_connected = // default 0
      //member->expiry_time  = // default 0
      //member->expires_at   = // default 0
  
      /* only used for local member */
      //member->nak_holdoff  = // default 0
      //member->last_acked   = // default 0
      //member->nak_modulus  = // default 0
      //member->nak_max_wait  = // default 0
      return member;
    }
  }
  printf("sdtm_add_member: none left\n");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Remove local member
*/
sdt_member_t *
sdtm_remove_member(sdt_channel_t *channel, sdt_member_t *member)
{
  sdt_member_t *cur;

  /* if it is at top, then we just move leader */
  if (member == channel->member_list) {
    channel->member_list = member->next;
    member->component = NULL; /* mark it empty */
    return channel->member_list;
  }
  
  /* else run through list in order, finding previous one */
  cur = channel->member_list;
  while (cur) {
    if (cur->next == member) {
      /* jump around it */
      cur->next = member->next;
      member->component = NULL; /* mark it empty */
      return cur->next;
    }
    cur = cur->next;
  }
  return NULL;
}

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
    /* we are going to assume that we will alsway find one! */
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
sdtm_add_component(cid_t cid, cid_t dcid, bool is_local)
{
  component_t *component;

  /* find and empty one */
  for (component = components_m; component < components_m + SDT_MAX_COMPONENTS; component++) {
		if (uuidIsNull(component->cid)) {
      /* clear values */
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
      return component;
    }
  }
  printf("sdtm_add_component: none left\n");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
    This does not release:
     the members in the member_list
     the tx_channel
     the tx_channel->sock
*/
component_t *
sdtm_remove_component(component_t *component)
{
  component_t *cur;

  /* if it is at top, then we just move leader */
  if (component == components) {
    components = component->next;
    uuidNull(component->cid); /* mark it empty */
    return components;
  }
  
  /* else run through list in order, finding previous one */
  cur = components;
  while (cur) {
    if (cur->next == component) {
      /* jump around it */
      cur->next = component->next;
      uuidNull(component->cid); /* mark it empty */
      return (cur->next);
    }
    cur = cur->next;
  }
  return NULL;
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
sdtm_find_component(cid_t cid)
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
#endif

#if CONFIG_SDTMEM_MALLOC
#error CONFIG_SDTMEM_MALLOC not implemented
#endif /* CONFIG_SDTMEM_MALLOC */
