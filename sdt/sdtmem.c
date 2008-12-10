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
/* static const char *rcsid __attribute__ ((unused)) = */
/*   "$Id$"; */

#include "opt.h"
#include "types.h"
#include "acn_arch.h"

#include "sdt.h"
#include "sdtmem.h"
#include "acnlog.h"

#if CONFIG_SDTMEM_STATIC
static sdt_channel_t    channels_m[SDT_MAX_CHANNELS];
static sdt_member_t     members_m[SDT_MAX_MEMBERS];
static component_t      components_m[SDT_MAX_COMPONENTS];
static sdt_resend_t     resends_m[SDT_MAX_RESENDS];  /* list of buffers */

#endif

#if CONFIG_SDTMEM_STATIC
/*****************************************************************************/
void
sdtm_init(void)
{
	sdt_channel_t   *channel;
  sdt_member_t    *member;
  component_t     *component;
  sdt_resend_t       *resend;
	for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; ++channel) channel->number = 0;
	for (member = members_m; member < members_m + SDT_MAX_MEMBERS; ++member) member->component = NULL;
	for (component = components_m; component < components_m + SDT_MAX_COMPONENTS; ++component) uuidNull(component->cid);
	for (resend = resends_m; resend < resends_m + SDT_MAX_RESENDS; ++resend) resend->expires_ms = 0;
}    

/*****************************************************************************/
/*
  Create a new channel
*/
sdt_channel_t *
sdtm_new_channel(void)
{
	sdt_channel_t *channel;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_channel");

  /* find and empty one */
  for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; channel++) {
		if (channel->number == 0) {
      /* clear values */
      memset(channel, 0, sizeof(sdt_channel_t));
      return channel;
    }
  }
  acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_channel: none left");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused channel
*/
void 
sdtm_free_channel(sdt_channel_t *channel)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_channel");

  /* mark channel as unused */
  channel->number = 0; /* mark it empty */
  return;
}


/*****************************************************************************/
/*
  Create a new member 
*/
sdt_member_t *
sdtm_new_member(void)
{
  sdt_member_t *member;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_member");

  /* find and empty one */
  for (member = members_m; member < members_m + SDT_MAX_MEMBERS; member++) {
		if (member->component == NULL) {
      /* clear values */
      memset(member, 0, sizeof(sdt_member_t));
      return member;
    }
  }
  acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_member: None left");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused member
*/
void
sdtm_free_member(sdt_member_t *member)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_member");
  member->component = NULL; /* mark it empty */
}

/*****************************************************************************/
/*
  Create a new component
*/
component_t *
sdtm_new_component(void)
{
  component_t *component;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_component");

  /* find and empty one */
  for (component = components_m; component < components_m + SDT_MAX_COMPONENTS; component++) {
		if (uuidIsNull(component->cid)) {
      /* clear values */
      memset(component, 0, sizeof(component_t));
      return component;
    }
  }
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_component: none left");

  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused component
*/
void
sdtm_free_component(component_t *component)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_component");
  uuidNull(component->cid); /* mark it empty */
}

/*****************************************************************************/
/*
  Create a new resend buffer
*/
sdt_resend_t *
sdtm_new_resend(void)
{
	sdt_resend_t *resend;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_resend");

  /* find and empty one */
  for (resend = resends_m; resend < resends_m + SDT_MAX_RESENDS; resend++) {
		if (resend->tx_buffer == NULL) {
      /* clear values */
      memset(resend, 0, sizeof(sdt_resend_t));
      return resend;
    }
  }
  acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_resend: none left");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Free an unused resend
*/
void
sdtm_free_resend(sdt_resend_t *resend)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_resend");
  
  resend->tx_buffer = NULL;                           /* mark it empty */
}

#endif /* of CONFIG_SDTMEM_STATIC */


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
sdtm_new_channel(void)
{
  sdt_channel_t *channel;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_channel");

  channel = malloc(sizeof(sdt_channel_t));
  if(!channel){
    acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_channel: Out of Memory");
    return NULL;
  }
  memset(channel,0,sizeof(sdt_channel_t));  
  return channel;
}
  
/*****************************************************************************/
/*
  Free an unused channel
*/
void 
sdtm_free_channel(sdt_channel_t *channel)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_channel");
  
  /* mark channel as unused */
  free(channel);
  return;
}
  
/*****************************************************************************/
/*
  Create a new member 
*/
sdt_member_t *
sdtm_new_member(void)
{
  sdt_member_t *member;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_member");

  member = malloc(sizeof(sdt_member_t));
    
  if(!member){
    acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_member: Out of memory.");
    return NULL;
  }
  memset(member,0,sizeof(sdt_member_t));
  return member;
}


/*****************************************************************************/
/*
  Free an unused member
*/
void
sdtm_free_member(sdt_member_t *member)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_member");
  free(member);
}

/*****************************************************************************/
/*
  Create a new component
*/
component_t *
sdtm_new_component(void)
{
  
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_component");
    
  component = malloc(sizeof(component_t));
  if(!component){
    acnlog(LOG_ERR | LOG_SDTM,"sdtm_new_component: Out of memory.");
    return NULL;
  }
  memset(component, 0, sizeof(component_t));
  return component;
}

/*****************************************************************************/
/*
  Free an unused component
*/
void
sdtm_free_component(component_t *component)
{
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_component");
  free(component);
}

#endif /* CONFIG_SDTMEM_MALLOC */


