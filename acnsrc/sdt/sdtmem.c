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

#tabs=2s
*/
/*--------------------------------------------------------------------*/
/*
This module is separated from the main SDT to allow customization of memory
handling.

*/
/* static const char *rcsid __attribute__ ((unused)) = */
/*   "$Id$"; */

#include "opt.h"
#if CONFIG_SDT
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include "sdt.h"
#include "sdtmem.h"

#if CONFIG_SDTMEM == MEM_STATIC
static sdt_channel_t    channels_m[SDT_MAX_CHANNELS];
static sdt_member_t     members_m[SDT_MAX_MEMBERS];
static sdt_resend_t     resends_m[SDT_MAX_RESENDS];  /* list of buffers */
#endif

#if CONFIG_SDTMEM == MEM_STATIC
/*****************************************************************************/
void
sdtm_init(void)
{
  sdt_channel_t   *channel;
  sdt_member_t    *member;
  sdt_resend_t    *resend;
  static bool initialized_state = 0;

  if (initialized_state) {
    acnlog(LOG_INFO | LOG_SDTM,"sdtm_init: already initialized");
    return;
  }
  initialized_state = 1;

  for (channel = channels_m; channel < channels_m + SDT_MAX_CHANNELS; ++channel) channel->number = 0;
  for (member = members_m; member < members_m + SDT_MAX_MEMBERS; ++member) member->component = NULL;
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
  Create a new resend buffer
*/
sdt_resend_t *
sdtm_new_resend(void)
{
  sdt_resend_t *resend;

  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_new_resend...");

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
  acnlog(LOG_DEBUG | LOG_SDTM,"sdtm_free_resend...");

  resend->tx_buffer = NULL;                           /* mark it empty */
}

/*****************************************************************************/
/*
  Free all resends
*/
void
sdtm_free_resends(void)
{
  sdt_resend_t *resend;

  acnlog(LOG_DEBUG | LOG_SDTM,"void_free_resends...");

  for (resend = resends_m; resend < resends_m + SDT_MAX_RESENDS; resend++) {
    resend->tx_buffer = NULL;
  }
}

#elif CONFIG_SDTMEM == MEM_MALLOC
/*****************************************************************************/
/*
  Free all resends
*/
extern sdt_resend_t *resends;

void sdtm_free_resends(void)
{
  sdt_resend_t *resend, *next;

  acnlog(LOG_DEBUG | LOG_SDTM,"void_free_resends...");

  for (resend = resends; resend != NULL; resend = next) {
    next = resend->next;
    free(resend);
  }
}

#endif /* CONFIG_SDTMEM == MEM_MALLOC */

#endif /* #if CONFIG_SDT */
