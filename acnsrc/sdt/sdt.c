/*

Copyright (c) 2007, Pathway Connectivity Inc.
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


TODO: - REMOVE DMP dependancy if DMP is not enabled.
      - When receiving ACK we can delete packets in resend buffer that no longer needed
        by and member.
      - Add stats to layers (packets: total, resent, missed, sdt, rlp, dmp, other, reliable, unreliable)
      - Implement missing functions (params, sessions)
      - SDT uses SDT_ADHOC_PORT for it's port but it should be using an ephemeral port...(netx_PORT_EPHEM)
        To do this we need to insure that we have our port established before we register our components.


Notes:
  Threading: see notes at sdt_tick();

#tabs=2s
*/
/*--------------------------------------------------------------------*/
/* static const char *rcsid __attribute__ ((unused)) = */
/*   $Id$ */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>  /* for rand()   */
#include <assert.h>  /* for assert() */

#include "opt.h"
#if CONFIG_SDT
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"
#include "acn_arch.h"
#include "netxface.h"   /* access to our network interface */

#include "sdt.h"        /* SDT public header */
#include "sdtmem.h"     /* SDT memory management */
#include "acn_sdt.h"
#include "rlp.h"        /* RLP layer */
#include "rlpmem.h"     /* RLP memory layer */
#include "acnlog.h"     /* acn log functions */
#include "marshal.h"    /* machine independent packing and unpacking of data */
#include "mcast_util.h" /* handy multicast utilities */
#include "ntoa.h"

#if CONFIG_STACK_LWIP
#include "lwip/sys.h"
#endif

/* this will/should go away as DMP should register itself with SDT so there are no dependencies */
#if CONFIG_DMP
  #include "dmp.h"
#endif

/* local enums/defines */
#if !CONFIG_RLP_SINGLE_CLIENT
/*
if your compiler does not conform to ANSI macro expansions it may
recurse infinitely on this definition.
*/
#define rlp_add_pdu(buf, pdudata, size, packetdatap) rlp_add_pdu(buf, pdudata, size, SDT_PROTOCOL_ID, packetdatap)
#endif

/*
macros for deep debugging - log entry and exit to each function
redefine these to empty macros (uncomment the ones below) to disable
*/
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_SDT, "%s [", __func__)
#define LOG_FEND() acnlog(LOG_DEBUG | LOG_SDT, "%s ]", __func__)
/*
#define LOG_FSTART()
#define LOG_FEND()
*/

/* keeps track of things we have allocated in case we need to abort, we can deallocate them */
enum
{
  ALLOCATED_LOCAL_COMP       = 1,
  ALLOCATED_LOCAL_CHANNEL    = 2,
  ALLOCATED_LOCAL_MEMBER     = 4,
  ALLOCATED_FOREIGN_COMP     = 8,
  ALLOCATED_FOREIGN_CHANNEL  = 16,
  ALLOCATED_FOREIGN_MEMBER   = 32,
  ALLOCATED_FOREIGN_LISTENER = 64
};

/* local variables */
       sdt_resend_t   *resends = NULL;              /* list of buffers to resend */
       netxsocket_t   *sdt_adhoc_socket = NULL;     /* socket for adhock */
struct rlp_listener_s *sdt_adhoc_listener = NULL;   /* listener for RLP */
       netxsocket_t   *sdt_multicast_socket = NULL; /* socket for ACN multcast */
sdt_state_t            sdt_state;                   /* state of unit */

/* BASE MESSAGES */
static int      sdt_tx_join(component_t *local_component, component_t *foreign_component, bool is_reciprocal);
static int      sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component);
static int      sdt_tx_join_refuse(const cid_t foreign_cid, component_t *local_component, const netx_addr_t *source_addr,
                   uint16_t foreign_channel_num, uint16_t local_mid, uint32_t foreign_rel_seq, uint8_t reason);
static int      sdt_tx_leaving(component_t *foreign_component, component_t *local_component, sdt_member_t *local_member, uint8_t reason);
static int      sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed);
/* TODO:         sdt_tx_get_sessions() */
/* TODO:         sdt_tx_sessions() */

static void     sdt_rx_join(const cid_t foreign_cid, const netx_addr_t *source_addr, const uint8_t *join, uint32_t data_len);
static void     sdt_rx_join_accept(const cid_t foreign_cid, const uint8_t *join_accept, uint32_t data_len);
static void     sdt_rx_join_refuse(const cid_t foreign_cid, const uint8_t *join_refuse, uint32_t data_len);
static void     sdt_rx_leaving(const cid_t foreign_cid, const uint8_t *leaving, uint32_t data_len);
static void     sdt_rx_nak(const cid_t foreign_cid, const uint8_t *nak, uint32_t data_len);
static void     sdt_rx_wrapper(const cid_t foreign_cid, const netx_addr_t *source_addr, const uint8_t *wrapper,  bool is_reliable, uint32_t data_len, void *ref);
/* TODO:         sdt_rx_get_sessions() */
/* TODO:         sdt_rx_sessions() */

/* WRAPPED MESSAGES */
static int      sdt_tx_ack(component_t *local_component, component_t *foreign_component);
/* TODO:         sdt_tx_channel_params(sdt_wrapper_t *wrapper); */
static int      sdt_tx_leave(component_t *local_component, component_t *foreign_component, sdt_member_t *foreign_member);
static int      sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
static int      sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol);
/* TODO:         sdt_tx_connect_refuse(sdt_wrapper_t *wrapper); */
static int      sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
static int      sdt_tx_disconnecting(component_t *local_component, component_t *foreign_component, uint32_t protocol, uint8_t reason);
static int      sdt_tx_mak_all(component_t *local_component);
static int      sdt_tx_mak_one(component_t *local_component, component_t *foreign_component);


static void     sdt_rx_ack(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
/* TODO:         sdt_rx_channel_params(); */
static void     sdt_rx_leave(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect_refuse(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_disconnect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_disconnecting(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);

/* UTILITY FUNCTIONS */
/* some of these are commented out as they were moved to the puplic header */
static       uint8_t *marshal_channel_param_block(uint8_t *paramBlock);
static const uint8_t *unmarshal_channel_param_block(sdt_member_t *member, const uint8_t *param_block);
static       uint8_t *marshal_transport_address(uint8_t *data, const netx_addr_t *transport_addr, int type);
static const uint8_t *unmarshal_transport_address(const uint8_t *data, const netx_addr_t *transport_addr, netx_addr_t *packet_addr, uint8_t *type);
/* static       uint8_t *sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold); */
static       void     sdt_client_rx_handler(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static       uint32_t check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail);
/* static       uint8_t *sdt_format_client_block(uint8_t *client_block, uint16_t foreignlMember, uint32_t protocol, uint16_t association); */

static uint16_t      sdt_next_member(sdt_channel_t *channel);
/* static sdt_member_t *sdt_find_member_by_mid(sdt_channel_t *channel, uint16_t mid); */
/* static sdt_member_t *sdt_find_member_by_component(sdt_channel_t *channel, component_t *component); */

static int sdt_save_buffer(rlp_txbuf_t *tx_buffer, uint8_t *pdup, uint32_t size, component_t *local_component);
static int sdt_tx_resend(sdt_resend_t *resend);
static int sdt_flag_for_resend(sdt_channel_t *channel, uint32_t reliable_seq);

/*****************************************************************************/
/* may go away, used for testing */
int
sdt_get_adhoc_port(void)
{
  return NSK_PORT(sdt_adhoc_socket);
}

/*****************************************************************************/
/*
  Initialize SDT structures
  returns -1 if fails, 0 otherwise
 */
int
sdt_init(void)
{
  static bool initialized_state = 0;

  /* Prevent reentry */
  if (initialized_state) {
    acnlog(LOG_INFO | LOG_SDT,"sdt_init: already initialized");
    return FAIL;
  }
  LOG_FSTART();

  comp_start();
  rlp_init();
  sdtm_init();

  initialized_state = 1;
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Short wrapper around join command with some testing...
 */
int
sdt_join(component_t *local_component, component_t *foreign_component)
{
  sdt_member_t  *local_member;
  sdt_member_t  *foreign_member;
  acn_protect_t  protect;

  if (local_component && foreign_component) {
    protect = ACN_PORT_PROTECT();
    if (local_component->tx_channel) {
      foreign_member = sdt_find_member_by_component(local_component->tx_channel,foreign_component);
      if (foreign_member) {
        acnlog(LOG_INFO | LOG_SDT,"sdt_join: already member");
        return FAIL;
      }
    }
    if (sdt_tx_join(local_component, foreign_component, false) == FAIL) {
      ACN_PORT_UNPROTECT(protect);
      return FAIL;
    }
    local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
    foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
    if ((!local_member) || (!foreign_member)) {
      ACN_PORT_UNPROTECT(protect);
      return FAIL;
    }
    foreign_member->state = msWAIT_FOR_ACCEPT;
    local_member->state = msEMPTY;
    ACN_PORT_UNPROTECT(protect);
    return OK;
  }
  return FAIL;
}

/*****************************************************************************/
/*
  Short wrapper around leave command with some testing...
 */
int
sdt_leave(component_t *local_component, component_t *foreign_component)
{
  sdt_member_t  *local_member;
  sdt_member_t  *foreign_member;
  acn_protect_t  protect;

  if (local_component && foreign_component) {
    protect = ACN_PORT_PROTECT();
    if (local_component->tx_channel) {
      foreign_member = sdt_find_member_by_component(local_component->tx_channel,foreign_component);
      if (!foreign_member) {
        ACN_PORT_UNPROTECT(protect);
        acnlog(LOG_INFO | LOG_SDT,"sdt_leave: already member");
        return FAIL;
      }
    } else {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_INFO | LOG_SDT,"sdt_leave: channel has no members");
      return FAIL;
    }

    if (sdt_tx_leave(local_component, foreign_component, foreign_member) == FAIL) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_INFO | LOG_SDT,"sdt_leave: sdt_tx_leave failed");
      return FAIL;
    }

    local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
    if (!local_member) {
      acnlog(LOG_INFO | LOG_SDT,"sdt_leave: missing local member?");
    }
    local_member->state = msDELETE;

    foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
    if (!foreign_member) {
      acnlog(LOG_INFO | LOG_SDT,"sdt_leave: missing foreign member?");
    }
    foreign_member->state = msDELETE;

    ACN_PORT_UNPROTECT(protect);
    return OK;
  }
  return FAIL;
}


/*****************************************************************************/
/*
  Kick off the SDT processes
    returns -1 if fails, 0 otherwise
 */
int
sdt_startup(bool acceptAdHoc)
{
  localaddr_t localaddr;

  LOG_FSTART();

  /* prevent reentry */
  if (sdt_state != ssCLOSED) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_startup: already started");
    return OK;
  }

  if (acceptAdHoc) {
    /* if an SDT multicast socket has not been opened yet */
    if (!sdt_multicast_socket) {
      LCLAD_PORT(localaddr) = htons(SDT_MULTICAST_PORT);
      sdt_multicast_socket = rlp_open_netsocket(&localaddr);
      if (!sdt_multicast_socket) {
        acnlog(LOG_ERR | LOG_SDT, "sdt_startup: unable to open multicast port");
        return FAIL;
      }
      /* Listeners for multicast will be added as needed */
    }
    /* Open ad hoc channel on ephemeral port */
    /* if the ad hoc socket is not open yet */
    if (!sdt_adhoc_socket) {
      /* TODO: this is just a hack until we get SLP working */
      LCLAD_PORT(localaddr) = htons(SDT_ADHOC_PORT);
      sdt_adhoc_socket = rlp_open_netsocket(&localaddr);
      /* sdt_adhoc_socket = rlp_open_netsocket(netx_PORT_EPHEM); */
      if (sdt_adhoc_socket) {
        sdt_adhoc_listener = rlp_add_listener(sdt_adhoc_socket, netx_GROUP_UNICAST, SDT_PROTOCOL_ID, sdt_rx_handler, NULL);
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_startup: adhoc port: %d", ntohs(NSK_PORT(sdt_adhoc_socket)));
      } else {
        acnlog(LOG_ERR | LOG_SDT, "sdt_startup: unable to open adhoc port");
        /* shut down multicast socket we opened above */
        rlp_close_netsocket(sdt_multicast_socket);
        sdt_multicast_socket = NULL;
        return -FAIL;
      }
    }
  }
  sdt_state = ssSTARTED;

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Shutdown SDT process
    returns -1 if fails, 0 otherwise
 */
int
sdt_shutdown(void)
{
  component_t    *component;
  sdt_member_t   *member;
  acn_protect_t   protect;
  sdt_resend_t   *resend;

  LOG_FSTART();

  /* Prevent reentry */
  if (sdt_state != ssSTARTED) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_shutdown: not started");
    return OK;
  }
  /* do this right away to prevent timer from hacking at these*/
  protect = ACN_PORT_PROTECT();
  sdt_state = ssCLOSING;

  /* if we have a local component with members in the channel, tell them  to get out of town */
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_shutdown: leaving sessions");
  for (component = allcomps; component != NULL; component = component->next) {
    if (component->tx_channel) {
      member = component->tx_channel->member_list;
      if (component->is_local) {
        /* local component, foreign members */
        while(member) {
          /* Notify DMP/Application */
          if (member->state == msCONNECTED) {
            if (member->component->callback)
              (*member->component->callback)(SDT_EVENT_DISCONNECT, component, member->component, false, NULL, 0, NULL);
            #if CONFIG_DMP
            /* FIXME: This should not be DMP dependent */
            sdt_tx_disconnect(component, member->component, DMP_PROTOCOL_ID);
            #endif
          }
          sdt_tx_leave(component, member->component, member);
          member = member->next;
        }
      } else {
        /* foreign component, local members */
        while(member) {
          /* Notify DMP/Application */
          if (member->state == msCONNECTED) {
            if (member->component->callback)
              (*member->component->callback)(SDT_EVENT_DISCONNECT, component, member->component, false, NULL, 0, NULL);
            #if CONFIG_DMP
            /* FIXME: This should not be DMP dependent */
            sdt_tx_disconnecting(component, member->component, DMP_PROTOCOL_ID, SDT_REASON_NONSPEC);
            #endif
          }
          sdt_tx_leaving(component, member->component, member, SDT_REASON_NONSPEC);
          member = member->next;
        }
      }
    }
  }

  /*  get first component and rip thru all of them (cleaning up resources along the way) */
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_shutdown: forgetting components");
  for (component = allcomps; component != NULL;) {
    component_t *nxtcomp = component->next;

    sdt_del_component(component);
    component = nxtcomp;
  }

  /* adhoc clean up */
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_shutdown: adhoc cleanup");
  if (sdt_adhoc_listener && sdt_adhoc_socket) {
    rlp_del_listener(sdt_adhoc_socket, sdt_adhoc_listener);
  }
  sdt_adhoc_listener = NULL;

  if (sdt_adhoc_socket) {
    rlp_close_netsocket(sdt_adhoc_socket);
  }
  sdt_adhoc_socket = NULL;

  /* multicast clean up.  listeners for multicast are cleaned up with the associated channel */
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_shutdown: multicast cleanup");
  if (sdt_multicast_socket) {
    rlp_close_netsocket(sdt_multicast_socket);
  }
  sdt_multicast_socket = NULL;

  /* free any memory used by resends and the resends */
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_shutdown: free resend memory");
  resend = resends;
  while (resend) {
    if (resend->tx_buffer) {
      rlpm_release_txbuf(resend->tx_buffer);
    }
    resend = resend->next;
  }
  sdtm_free_resends();

  sdt_state = ssCLOSED;
  ACN_PORT_UNPROTECT(protect);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Add a new component and initialize it;
  returns new component or NULL
 */
#define ERROR_nomem       1
#define CONFLICT_notnew   2
#define CONFLICT_creator  4
#define CONFLICT_local    8
#define CONFLICT_dcid     16
#define CONFLICT_access   32

component_t *
sdt_add_component(const cid_t cid, const cid_t dcid, bool is_local, 
                  access_t access, created_by_t creator)
{
  component_t *component;
  acn_protect_t  protect;
#if acntestlog(LOG_DEBUG | LOG_SDT)
  char  cid_text[CID_STR_SIZE];
#endif
  int rslt = 0;

  assert(!cidIsNull(cid));
#if acntestlog(LOG_DEBUG | LOG_SDT)
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_add_component: %s", cidToText(cid, cid_text));
#endif

  protect = ACN_PORT_PROTECT();
  /* See if we have this already, otherwise get a new one */
  if ((component = comp_get_by_cid(cid)) == NULL)
    rslt = ERROR_nomem;
  else if (component->created_by) {  /* Already there */
    rslt = CONFLICT_notnew;
    if (component->created_by != creator) rslt |= CONFLICT_creator;
    if (component->is_local != is_local) {
      rslt |= CONFLICT_local;
      comp_release(component);
      component = NULL;
    }
    if (dcid) {
      if (cidIsNull(component->dcid)) cidCopy(component->dcid, dcid);
      else if (!cidIsEqual(dcid, component->dcid)) rslt |= CONFLICT_dcid;
    }
    if (access != accUNKNOWN) {
      if (component->access == accUNKNOWN) component->access = access;
      else if (component->access != access) rslt |= CONFLICT_access;
    }
  } else {
    /* initialise it */
    if (dcid) cidCopy(component->dcid, dcid);
    component->access = access;
    component->created_by = creator;
    if (component->is_local)
      mcast_alloc_init(0, 0, component);
  }
  ACN_PORT_UNPROTECT(protect);
  if (rslt) {
    if (rslt == ERROR_nomem) {
      acnlog(LOG_ERR | LOG_SDT,"sdt_add_component: new component allocation failed");
    } else {
      acnlog(LOG_INFO | LOG_SDT,"sdt_add_component: component already exists");
      if (rslt & CONFLICT_creator) {
        acnlog(LOG_INFO | LOG_SDT,"sdt_add_component: previous creator differed");
      }
      if (rslt & CONFLICT_local) {
        acnlog(LOG_ERR | LOG_SDT,"sdt_add_component: local/remote changed!");
      }
      if (rslt & CONFLICT_dcid) {
        acnlog(LOG_ERR | LOG_SDT,"sdt_add_component: DCID changed!");
      }
      if (rslt & CONFLICT_access) {
        acnlog(LOG_WARNING | LOG_SDT,"sdt_add_component: access type changed!");
      }
    }
  }
  return component;
}

/*****************************************************************************/
/*
  Delete a component and it's dependencies
  returns next component or NULL
 */
void
sdt_del_component(component_t *component)
{
  component_t   *cur;
  acn_protect_t  protect;
  sdt_member_t  *member;
#if acntestlog(LOG_DEBUG | LOG_SDT)
  char  cid_text[CID_STR_SIZE];
#endif

  assert(component);

#if acntestlog(LOG_DEBUG | LOG_SDT)
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_component: %s", cidToText(component->cid, cid_text));
#endif

  protect = ACN_PORT_PROTECT();
  /* remove this component from other components member list */
  for (cur = allcomps; cur != NULL; cur = cur->next) {
    if (cur->tx_channel) {
       member = sdt_find_member_by_component(cur->tx_channel, component);
       if (member) {
        acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_component: removing remote member");
        sdt_del_member(cur->tx_channel, member);
       }
    }
  }

  /* remove our channel (and member list) */
  if (component->tx_channel) {
    sdt_del_channel(component);
  }

  /* now free it */
  comp_release(component);

  ACN_PORT_UNPROTECT(protect);
}

/*****************************************************************************/
/*
  Externally exposed call to get first component in our component list;
*/
component_t *
sdt_first_component(void)
{
  return allcomps;
}

/*****************************************************************************/
/*
  Add a new channel into component and initialize it;
  returns new channel or NULL
 */
sdt_channel_t *
sdt_add_channel(component_t *leader, uint16_t channel_number)
{
  sdt_channel_t *channel;

  assert(leader);             /* must have a leader */

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_channel %d", channel_number);

  if (leader->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_add_channel: channel already exists");
    return leader->tx_channel;
  }

  /* We need to PROTECT this because incomming data is a higher priority and can add it's own channels */
  /* find and empty one */
  channel = sdtm_new_channel();
  if (channel) {
    /* assign this to our leader */
    /* assign passed and default values */
    channel->number  = channel_number;
    channel->available_mid     = 1;
    /* channel->total_seq       = */ /* default 0; */
    /* channel->reliable_seq    = */ /* default 0 */
    /* channel->oldest_avail    = */ /* default 0 */
    /* channel->member_list     = */ /* added when creating members */
    /* channel->sock;           = */ /* default null */
    /* channel->listener;       = */ /* multicast listener, default null */
    channel->mak_ms = FOREIGN_MEMBER_MAK_TIME_ms;
    leader->tx_channel = channel; /* put address of this chan structure into the component struct */
    return channel;
  }
  acnlog(LOG_ERR | LOG_SDT, "sdt_add_channel: failed to get new channel");
  return NULL; /* none left */
}

/*****************************************************************************/
/*
  Delete a channel and it's dependencies from a component
  returns NULL (in future,  this may return of next channel)
 */
sdt_channel_t *
sdt_del_channel(component_t *leader)
{
  sdt_member_t  *member;
  sdt_channel_t *channel;

  assert(leader);

  if (!leader->tx_channel) {
    acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_channel: component without channel");
    return NULL;
  }

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_channel: %d", leader->tx_channel->number);

  channel = leader->tx_channel;
  /* remove it from the leader */
  leader->tx_channel = NULL;

  /* now we can clean it up */
  /* remove members */
  if (channel) {
    member = channel->member_list;
    while(member) {
      member = sdt_del_member(channel, member);
    }
    /* remove listener */
    if (channel->listener) {
      rlp_del_listener(channel->sock, channel->listener);
    }
  }
  /* and nuke it */
  sdtm_free_channel(channel);

  return NULL;
}

/*****************************************************************************/
/*
  Add a new member into the channel and initialize it;
  returns new member or NULL
 */
sdt_member_t *
sdt_add_member(sdt_channel_t *channel, component_t *component)
{
  sdt_member_t *member;

  assert(channel);
  assert(component);

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_member: to channel %d", channel->number);

  /* verify the component does not already exist */
  member = channel->member_list;
  while (member) {
    if (member->component == component) {
      acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_member: component already member of channel");
      break;
    }
    member = member->next;
  }

  /* we need to protect this because incomming data is a higher priority and can add it's own members */
  /* find and empty one */
  member = sdtm_new_member();
  if (member) {
    /* put this one at the head of the list */
    member->next = channel->member_list;
    channel->member_list = member;
    /* assign passed and default values */
    member->component     = component;
    /* member->mid           = */ /* default 0 */
    /* member->nak           = */ /* default 0 */
    /* member->state         = */ /* deafult 0 = msEMPTY; */
    /* member->expiry_time_s = */ /* default 0 */
    member->expires_ms    = 5000;  /* default 0 */
    /* member->mak_retries   = */ /* default 0 */

    /* only used for local member */
    /* member->nak_holdoff  = */ /* default 0 */
    /* member->last_acked   = */ /* default 0 */
    /* member->nak_modulus  = */ /* default 0 */
    /* member->nak_max_wait  = */ /* default 0 */
    return member;
  }
  acnlog(LOG_ERR | LOG_SDT,"sdt_new_member: none left");

  return NULL; /* none left */
}


/*****************************************************************************/
/*
  Delete a member and it's dependencies from a channel
  returns next member or NULL
 */
sdt_member_t *
sdt_del_member(sdt_channel_t *channel, sdt_member_t *member)
{
  sdt_member_t *cur;

  assert(channel);
  assert(member);

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_member: %d from channel %d", member->mid, channel->number);

  /* if it is at top, then we just move leader */
  if (member == channel->member_list) {
    channel->member_list = member->next;
    sdtm_free_member(member);
    return channel->member_list;
  }

  /* else run through list in order, finding previous one */
  cur = channel->member_list;
  while (cur) {
    if (cur->next == member) {
      /* jump around it */
      cur->next = member->next;
      sdtm_free_member(member);
      return cur->next;
    }
    cur = cur->next;
  }
  return NULL;
}

/********************************************************************************
    Functions that do not depend on a particular memory model continue from here
*/

/*****************************************************************************/
/*
  find next available MID for a channel
*/
static uint16_t
sdt_next_member(sdt_channel_t *channel)
{

  assert(channel);

  if (channel->available_mid == 0x7FFF)
    channel->available_mid = 1;

  /* find MID typically it will the next MID */
  /* we are going to assume that we will always find one! */
  while(sdt_find_member_by_mid(channel, channel->available_mid))
    channel->available_mid++;

  /* return this one and go ahead in increment to next */
  return channel->available_mid++;
}

/*****************************************************************************/
/*
  Find member by it's MID
*/
/*static */sdt_member_t *
sdt_find_member_by_mid(sdt_channel_t *channel, uint16_t mid)
{
  sdt_member_t *member;

  assert(channel);

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
  Find member by it's component
*/
/*static*/  sdt_member_t *
sdt_find_member_by_component(sdt_channel_t *channel, component_t *component)
{
  sdt_member_t *member;

  assert(channel);
  assert(component);

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
  Find component by it's CID
*/
component_t *
sdt_find_comp_by_cid(const cid_t cid)
{
  component_t *component;

  for (component = allcomps; component != NULL; component = component->next)
    if (cidIsEqual(component->cid, cid))
      return (component);

  return NULL;  /* oops */
}

/*****************************************************************************/
/*
  Callback from ACN RLP layer (listener)
    data        - pointer to SDT packet
    data_len    - length of SDT packet
    ref         - value set when setting up listener (use use may vary with porting)
    remhost     - network address of sending (foreign) host
    foreign_cid - cid of sending component (pulled from root layer)

    NOTE: for lwip, ref is not the ref made from add_listener. It is pbuf used for reference counting,
 */
void
sdt_rx_handler(const uint8_t *data, int data_len, void *ref, const netx_addr_t *remhost, const cid_t foreign_cid)
{
  const uint8_t    *data_end;
  uint8_t           vector   = 0;
  uint32_t          data_size = 0;
  const uint8_t    *pdup, *datap = NULL;
  acn_protect_t     protect;

  LOG_FSTART();

  /* just in case something comes in while shutting down */
  if (sdt_state != ssSTARTED) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_handler: sdt not started");
    return;
  }

  /* local pointer */
  pdup = data;

  /* verify min data length */
  if (data_len < 3) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* On first packet, flags should all be set */
  /* TODO: support for long packets? */
  if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_handler: illegal first PDU flags");
    return;
  }

  /* Process all PDU's in root layer PDU*/
  while(pdup < data_end)  {
    const uint8_t *pp;
    uint8_t  flags;

    /* Decode the sdt base layer header, getting flags, length and vector */
    flags = unmarshalU8(pdup);

    /* save pointer, on second pass, we don't know if this is vector or data until we look at flags*/
    pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);

    /* fail if outside our packet */
    if (pdup > data_end) {
      acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_handler: packet length error");
      return;
    }

    /* Get vector or leave the same as last*/
    if (flags & VECTOR_bFLAG) {
      vector = unmarshalU8(pp);
    pp += sizeof(uint8_t);
    }
    /* pp now points to potential start of data */

    /* get packet data (or leave pointer to old data) */
    if (flags & DATA_bFLAG) {
      datap = pp;
      /* calculate reconstructed pdu length */
    data_size = pdup - pp;
    }

    protect = ACN_PORT_PROTECT();
    /* Dispatch to vector */
    /* At the sdt root layer vectors are commands or wrappers */
    switch(vector) {
      case SDT_JOIN :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_join");
        sdt_rx_join(foreign_cid, remhost, datap, data_size);
        break;
      case SDT_JOIN_ACCEPT :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_join_accept");
        sdt_rx_join_accept(foreign_cid, datap, data_size);
        break;
      case SDT_JOIN_REFUSE :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_join_refuse");
        sdt_rx_join_refuse(foreign_cid, datap, data_size);
        break;
      case SDT_LEAVING :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_leaving");
        sdt_rx_leaving(foreign_cid, datap, data_size);
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Done Dispatch");
        break;
      case SDT_NAK :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_nak");
        sdt_rx_nak(foreign_cid, datap, data_size);
        break;
      case SDT_REL_WRAPPER :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_wrapper as reliable");
        sdt_rx_wrapper(foreign_cid, remhost, datap, true, data_size, ref);
        break;
      case SDT_UNREL_WRAPPER :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_wrapper as unreliable");
        sdt_rx_wrapper(foreign_cid, remhost, datap, false, data_size, ref);
        break;
      case SDT_GET_SESSIONS :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Get Sessions unsupported");
        break;
      case SDT_SESSIONS :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Sessions??? I don't want any of that !");
        break;
      default:
        acnlog(LOG_WARNING | LOG_SDT,"sdtRxHandler: Unknown Vector (protocol) - skipping");
    } /* switch */
    ACN_PORT_UNPROTECT(protect);
  } /* while() */

  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Callback from OS to deal with session timeouts

  This function makes the assumption that other threads that use the memory allocated
  in sdtmem.c are of higher priority than this one and that a preemptive os is used.
  By doing this, the higher priority threads will complete what they are doing before
  giving up control.  This avoids more complex locking schemes. It also assumes that these
  tasks do not yield control back while in the middle of processing on the common memory.
*/
void
sdt_tick(void *arg)
{
  component_t   *component;
  component_t   *compnext;
  sdt_channel_t *channel;
  sdt_member_t  *member;
  acn_protect_t  protect;
  sdt_resend_t  *resend = NULL;
  sdt_resend_t  *last = NULL;

  UNUSED_ARG(arg);

  /* don't bother if we are not started */
  if (!sdt_state)
    return;

  protect = ACN_PORT_PROTECT();
  for (compnext = NULL, component = allcomps; component != NULL; component = compnext) {
    channel = component->tx_channel;

    if (channel) {

      member = channel->member_list;
      /* As a leader, we need to make sure everyone is still there. So we send a MAK every
       * once in a while. We expect a ACK in return. If not, we time out.
       * We only care about local leaders/foreign members
       *
       * As a member, we need to make sure the leader has sent us something every
       * once in a while. If not, we timeout.
       * We only care about foreign leaders/local members
       *
       * We use the same counter for both (expires_ms)
       */

      while(member) {
        /* delete any set in advance */
        if (member->state == msDELETE) {
          member = sdt_del_member(channel, member);
          continue;
        }
        /* otherwise process others */

        /* decrement timers to zero unless already negitive */
        if (member->expires_ms > 0) {
          member->expires_ms -= MS_PER_TICK;
          if (member->expires_ms < 0) {
            member->expires_ms = 0;
          }
        }

        /* expired if zero (or skipped if negitive) */
        if (member->expires_ms == 0) {
          /* if component is local, then all members of it's channel are foreign */
          if (component->is_local) {
            member->mak_retries--;
            if (member->mak_retries == 0) {
              /* foreign members in local component */
              /* send leave message*/
              acnlog(LOG_DEBUG | LOG_SDT,"std_tick: foreign exipred in our list");
              if (member->state == msCONNECTED) {
                if (member->component->callback)
                  (*member->component->callback)(SDT_EVENT_DISCONNECT, component, member->component, false, NULL, 0, NULL);
              }
              sdt_tx_leave(component, member->component, member);
              member->state = msDELETE;
            }
            member->expires_ms = -1;
          } else {
            /* local members in foreign component */
            /* send a leaving message */
            acnlog(LOG_DEBUG | LOG_SDT,"std_tick: local exipred in his list");
            if (member->state == msCONNECTED) {
              if (member->component->callback)
                (*member->component->callback)(SDT_EVENT_DISCONNECT, component, member->component, false, NULL, 0, NULL);
            }
            sdt_tx_leaving(component, member->component, member, SDT_REASON_CHANNEL_EXPIRED);
            member->state = msDELETE;
          }
        } /* if timer exired */

        /* delete member if marked above */
        if (member->state == msDELETE) {
          member = sdt_del_member(channel, member);
          continue;
        }
        member = member->next;
      } /* of while(member) */

      /* If we have no members left, delete the channel */
      if (!channel->member_list) {
        sdt_del_channel(component);
        /* if we had a channel but deleted it, the make sure we delete componnet below too */
        channel = NULL;
      } else {
        /* If we have members and the leader is local, check MAK timer and send if needed*/
        if (component->is_local) {
          /* decrement out MAK timer */
          channel->mak_ms -= MS_PER_TICK;
          if (channel->mak_ms < 0) {
            channel->mak_ms = 0;
          }
          /* If we have members and the leader is local, send MAK if mak time out     */
          if ((channel->mak_ms == 0)) {
            sdt_tx_mak_all(component);
            channel->mak_ms = FOREIGN_MEMBER_MAK_TIME_ms;
            /* reset timer on all members */
            member = channel->member_list;
            while (member) {
              member->expires_ms = MAK_TIMEOUT_ms;
              member = member->next;
            }
          } /* if time out */
        } /* if component is local */
      } /* if channel has members */
    } /* of if(channel) */

    compnext = component->next;
    /* delete self-created components that have no channel (and no members) */
    if (!channel && component->created_by == cbJOIN) sdt_del_component(component);
  } /* end while component */

  /* send any resends and flush any expired ones */
  resend = resends;
  while (resend) {
    /* This is a bit of a hack.  Our tick is 100ms but the NAK_BLANKTIME is 6ms */
    /* so we'll just use the the counter as a flag */
    if (resend->blanktime_ms) {
      sdt_tx_resend(resend);
      resend->blanktime_ms = 0;
    }

    resend->expires_ms -= MS_PER_TICK;
    if (resend->expires_ms <= 0) {
      /* acnlog(LOG_WARNING | LOG_SDT,"sdt_tick: timeout"); */
      /* top */
      if (resend == resends) {
        resends = resend->next;
        rlpm_release_txbuf(resend->tx_buffer);
        sdtm_free_resend(resend);
        resend = resends;
      } else {
        last->next = resend->next;
        rlpm_release_txbuf(resend->tx_buffer);
        sdtm_free_resend(resend);
        resend = last->next;
      }
    } else {
      last = resend;
      resend = resend->next;
    }
  }

  ACN_PORT_UNPROTECT(protect);
}

/*****************************************************************************/
/*
  Marshal the channel parameter block on to the data stream
    param_block - pointer to channel parameter block
    returns     - new pointer

  Currently these parameters are hard coded - if this changes this function will need more arguments
*/
static
uint8_t *
marshal_channel_param_block(uint8_t *param_block)
{
  param_block = marshalU8(param_block,  FOREIGN_MEMBER_EXPIRY_TIME_ms/1000);
  param_block = marshalU8(param_block,  0); /* suppress OUTBOUND NAKs from foreign members */
  param_block = marshalU16(param_block, FOREIGN_MEMBER_NAK_HOLDOFF_ms);
  param_block = marshalU16(param_block, FOREIGN_MEMBER_NAK_MODULUS);
  param_block = marshalU16(param_block, FOREIGN_MEMBER_NAK_MAX_TIME_ms);

  return param_block;
}

/*****************************************************************************/
/*
  Unmarshal the channel parameter block from the data stream
    member       - pointer to member to be populated
    param_block  - pointer to channel parameter block
    returns      - number of bytes marshaled.
*/
static const
uint8_t *
unmarshal_channel_param_block(sdt_member_t *member, const uint8_t *param_block)
{
  member->expiry_time_s = *param_block;
  param_block++;

  member->nak = *param_block;
  param_block++;
  member->nak_holdoff = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  member->nak_modulus = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  member->nak_max_wait = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  return param_block; /* length of the ParamaterBlock */
}

/*****************************************************************************/
/*
  Marshal transport address on to the data stream
    transport_addr - pointer to address
    returns        - new pointer
 */
static
uint8_t *
marshal_transport_address(uint8_t *data, const netx_addr_t *transport_addr, int type)
{
  switch(type) {
    case SDT_ADDR_NULL :
      data = marshalU8(data, SDT_ADDR_NULL);
      break;
    case SDT_ADDR_IPV4 :
      data = marshalU8(data, SDT_ADDR_IPV4);
      data = marshalU16(data, netx_PORT(transport_addr));
      data = marshalU32(data, htonl(netx_INADDR(transport_addr)));
      break;
    case SDT_ADDR_IPV6 :
      acnlog(LOG_WARNING | LOG_SDT,"marshal_transport_address: IPV6 not supported");
      break;
    default :
      acnlog(LOG_WARNING | LOG_SDT,"marshal_transport_address: upsupported address type");
  }
  return data;
}

/*****************************************************************************/
static const
uint8_t *
unmarshal_transport_address(const uint8_t *data, const netx_addr_t *transport_addr, netx_addr_t *packet_addr, uint8_t *type)
{
  /* get and save type */
*type = unmarshalU8(data);
  data += sizeof(uint8_t);

  switch(*type) {
    case SDT_ADDR_NULL :
      netx_PORT(packet_addr) = netx_PORT(transport_addr);
      netx_INADDR(packet_addr) = netx_INADDR(transport_addr);
      break;
    case SDT_ADDR_IPV4 :
      netx_PORT(packet_addr) = unmarshalU16(data);
      data += sizeof(uint16_t);
      netx_INADDR(packet_addr) = htonl(unmarshalU32(data));
      data += sizeof(uint32_t);
      break;
    case SDT_ADDR_IPV6 :
      acnlog(LOG_WARNING | LOG_SDT,"unmarshal_transport_address: IPV6 not supported");
      break;
    default :
      acnlog(LOG_WARNING | LOG_SDT,"unmarshal_transport_address: upsupported address type");
  }
  return data;
}

/*****************************************************************************/
/*
  Create a SDT_REL_WRAPPER or SDT_UNREL_WRAPPER
    wrapper        - pointer to wrapper
    is_reliable    - boolean if wrapper is reliable
    local_channel  - channel the wrapper is for
    first_mid      - first MID to ack
    last_mid       - last MID to ack
    mak_threshold  - MAK threshold
    returns        - pointer to start of client block
*/
/*static */
uint8_t *
sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold)
{
  static int sent_reliable = 0;

  assert(local_channel);

  LOG_FSTART();

  /* Skip flags/length fields for now*/
  wrapper += sizeof(uint16_t);

  /* incremenet our sequence numbers... */
  if (is_reliable) {
    /* set next sequence number */
    local_channel->reliable_seq++;
    if (sent_reliable) {
    /* calculate oldest based on buffer size */
    local_channel->oldest_avail = (local_channel->reliable_seq - local_channel->oldest_avail == SDT_MAX_RESENDS) ?
                                   local_channel->reliable_seq - SDT_MAX_RESENDS + 1 : local_channel->oldest_avail;
    } else {
      local_channel->oldest_avail = local_channel->reliable_seq;
      sent_reliable = 1;
    }

  }
  local_channel->total_seq++;

  /* wrawpper type */
  wrapper = marshalU8(wrapper, (is_reliable) ? SDT_REL_WRAPPER : SDT_UNREL_WRAPPER);
  /* channel number */
  wrapper = marshalU16(wrapper, local_channel->number);
  wrapper = marshalU32(wrapper, local_channel->total_seq);
  wrapper = marshalU32(wrapper, local_channel->reliable_seq);

  /* FIXME - Change to oldest available when buffering support added */
  wrapper = marshalU32(wrapper, local_channel->oldest_avail);
  wrapper = marshalU16(wrapper, first_mid);
  wrapper = marshalU16(wrapper, last_mid);
  wrapper = marshalU16(wrapper, mak_threshold);

  /* return pointer to client block */
  LOG_FEND();
  return wrapper;
}


/*****************************************************************************/
/*
  Procsss SDT channel packets
    local_component   - compoent receiving the packet
    foreign_component - component that sent the packet
    data              - ponter to client block
    data_len          - length of client packet
 */
static void
sdt_client_rx_handler(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  const uint8_t    *data_end;
  const uint8_t    *pdup, *datap = NULL;
  uint8_t           vector   = 0;
  uint32_t          data_size = 0;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  /* verify min data length */
  if (data_len < 3) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_client_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* start of our pdu block containing one or multiple pdus*/
  pdup = data;

  if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_client_rx_handler: illegal first PDU flags");
    return;
  }

  while(pdup < data_end)  {
    const uint8_t *pp;
    uint8_t  flags;

    /* Decode flags */
    flags = unmarshalU8(pdup);

    /* save pointer*/
    pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);

    if (pdup > data_end) {
      acnlog(LOG_WARNING | LOG_SDT,"sdt_client_rx_handler: packet length error");
      return;
    }

    /* Get vector or leave the same as last */
    if (flags & VECTOR_bFLAG) {
      vector = unmarshalU8(pp);
      pp += sizeof(uint8_t);
    }
    /* pp now points to potential start of data */

    /* get packet data (or leave pointer to old data) */
    if (flags & DATA_bFLAG) {
      datap = pp;
      /* calculate reconstructed pdu length */
      data_size = pdup - pp;
    }


    switch(vector) {
      case SDT_ACK :
        sdt_rx_ack(local_component, foreign_component, datap, data_size);
        break;
      case SDT_CHANNEL_PARAMS :
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: SDT_CHANNEL_PARAMS not supported..");
        break;
      case SDT_LEAVE :
        sdt_rx_leave(local_component, foreign_component, datap, data_size);
        break;
      case SDT_CONNECT :
        sdt_rx_connect(local_component, foreign_component, datap, data_size);
        break;
      case SDT_CONNECT_ACCEPT :
        sdt_rx_connect_accept(local_component, foreign_component, datap, data_size);
        break;
      case SDT_CONNECT_REFUSE :
        sdt_rx_connect_refuse(local_component, foreign_component, datap, data_size);
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: Our Connect was Refused ??? ");
        break;
      case SDT_DISCONNECT :
        sdt_rx_disconnect(local_component, foreign_component, datap, data_size);
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: rx sdt DISCONNECT (NOP)");
        break;
      case SDT_DISCONNECTING :
        sdt_rx_disconnecting(local_component, foreign_component, datap, data_size);
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: rx sdt DISCONNECTING (isn't that special)");
        break;
      default :
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: Unknown Vector -- Skipping");
    }
  }
  LOG_FEND();
}

/*****************************************************************************/
/*
  Check to see if wrapped packet is sequenced correctly
    channel       - channel to test against
    is_reliable   - is received packet reliable
    total_seq     - packets total seq value
    reliable_seq  - packets reliable seq value
    oldest_avail  - packets oldest avail value
    returns       - SEQ_VALID if packet is correctly sequenced or error code

    note: there is a case where we could miss a reliable packet followed by a unreliable packet.
          In this case, we would detect the missed packet from the info in the unreliable packet
          but the unreliable packet itself is OK.  This routine returns a SEQ_NAK in this case so
          calling process must still process the packet if unreliable (and send a NAK)
*/
static uint32_t
check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail)
{
  int diff;

  assert(channel);


  /* update oldest */
  channel->oldest_avail = oldest_avail;

  diff = total_seq - channel->total_seq;
  if (diff == 1) {
    /* normal seq */
    acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: ok");
    /* update channel object */
    channel->total_seq = total_seq;
    channel->reliable_seq = reliable_seq;
    return SEQ_VALID; /* packet should be processed */
  } else {
    if (diff > 1) {
      /* missing packet(s) check reliable */
      diff = reliable_seq - channel->reliable_seq;
      if ((is_reliable) && (diff == 1))  {
        acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: missing unreliable");
        /* update channel object */
        channel->total_seq = total_seq;
        channel->reliable_seq = reliable_seq;
        return SEQ_VALID;
      } else {
        if (diff) { /*not equal */
          /* missed reliable packets */
          diff = oldest_avail - channel->reliable_seq;
          if (diff > 1) {
            acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: sequence lost");
            return SEQ_LOST;
          } else {
            acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: sequence nak");
            return SEQ_NAK;/* activate nak system */
          }
        } else {
          /* missed unreliable packets */
          /* update channel object */
          channel->total_seq = total_seq;
          channel->reliable_seq = reliable_seq;
          return SEQ_VALID;
        }
      }
    } else {
      if (diff == 0)
        /* old packet or duplicate - discard */
        acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: duplicate");
        return SEQ_DUPLICATE;
      /* else diff < 0 */
      acnlog(LOG_DEBUG | LOG_SDT, "check_sequence: old packet");
      return SEQ_OLD_PACKET;
    }
  }
}

/*****************************************************************************/
/*
  Format a client block into the currrent SDT wrapper.
    client_block - pointer to client block data stream
    foreign_mid  - member id
    protocol     - protocol
    association  - association
    returns      - new stream pointer (to opaque datagram)
*/
/*static*/
uint8_t *
sdt_format_client_block(uint8_t *client_block, uint16_t foreign_mid, uint32_t protocol, uint16_t association)
{

  assert(client_block);
  /* LOG_FSTART(); */

  /* skip flags and length for now */
  client_block += 2;

  client_block = marshalU16(client_block, foreign_mid);   /* Vector */
  client_block = marshalU32(client_block, protocol);      /* Header, Client protocol */
  client_block = marshalU16(client_block, association);   /* Header, Association */

  /* LOG_FEND(); */

  /* returns pointer to opaque datagram */
  return client_block;
}

/*****************************************************************************/
/*
  Convert seconds to system units
 */
static __inline
uint32_t
sec_to_ticks(int time_in_sec)
{
  return time_in_sec * 1000;
}

static __inline
uint32_t
msec_to_ticks(int time_in_msec)
{
  return time_in_msec;
}


/*****************************************************************************/
/*
  Send JOIN
  local_component    - local component
  foreign_compoinent - foreign component

  local_component will need a channel assinged to tx_channel. If one is not provided
  a multicast destination will be created.

  local_channel will need a socket to send on. If one is not provided, the ad-hoc socket
  will be used

  foreign_component needs either a tx_channel=>source_addr or have it's ad-hoc address set.
    For new joins, it would tx_channel would normally nil and we would use the ad-hoc address from SLP
    For reciprocol joins, a tx_channal would be created in the rx_join and used here
*/
int
sdt_tx_join(component_t *local_component, component_t *foreign_component, bool is_reciprocal)
{
  sdt_member_t  *local_member = NULL;
  sdt_member_t  *foreign_member = NULL;
  sdt_channel_t *local_channel = NULL;
  sdt_channel_t *foreign_channel = NULL;
  uint8_t *buf_start;
  uint8_t *buffer;
  int      allocations = 0;
  rlp_txbuf_t *tx_buffer;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

/*  auto void remove_allocations(void); */
/*  void remove_allocations(void) { */
#undef remove_allocations
#define remove_allocations() \
    if (allocations & ALLOCATED_LOCAL_MEMBER) sdt_del_member(foreign_channel, local_member); \
/*    if (allocations & ALLOCATED_FOREIGN_LISTENER) rlp_del_listener(sdt_multicast_socket, foreign_channel->listener); */ \
    if (allocations & ALLOCATED_FOREIGN_CHANNEL) sdt_del_channel(foreign_component); \
/*    if (allocations & ALLOCATED_FOREIGN_COMP) sdt_del_component(foreign_component); */ \
    if (allocations & ALLOCATED_FOREIGN_MEMBER) sdt_del_member(local_channel, foreign_member); \
    if (allocations & ALLOCATED_LOCAL_CHANNEL)  sdt_del_channel(local_component);
/*  } */

  local_channel = local_component->tx_channel;
  foreign_channel = foreign_component->tx_channel;

  /* create local channel */
  if (!local_channel) {
    /* create channel */
    local_channel = sdt_add_channel(local_component, rand());
    if (!local_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : unable to add local channel");
      return FAIL;
    }
    allocations |= ALLOCATED_LOCAL_CHANNEL;
    /* assign channel address */
    netx_PORT(&local_channel->destination_addr) = htons(SDT_MULTICAST_PORT);
    netx_INADDR(&local_channel->destination_addr) = mcast_alloc_new(local_component);
  }

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_join : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create foreign member */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    foreign_member = sdt_add_member(local_channel, foreign_component);
    if (!foreign_member) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : failed to allocate foreign member");
      remove_allocations();
      return FAIL;
    }
    allocations |= ALLOCATED_FOREIGN_MEMBER;
  } else {
    if (foreign_member->state != msEMPTY) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : already member");
      return FAIL;
    }
  }

  /* set mid */
  foreign_member->mid = sdt_next_member(local_channel);
  /* Timeout if JOIN ACCEPT and followup ACK is not received */
  foreign_member->expires_ms = RECIPROCAL_TIMEOUT_ms;
  foreign_member->mak_retries = MAK_MAX_RETRIES;

  /* create foreign channel */
  if (!foreign_channel) {
    /* create a channel for the recipocal join*/
    /* note, we are using a channel numeber of 0 as this gets populated with
     * the real number on the reciprocal join/accept */
    foreign_channel = sdt_add_channel(foreign_component, 0);
    if (!foreign_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : unable to add foreign channel");
      remove_allocations();
      return FAIL;
    }
    allocations |= ALLOCATED_FOREIGN_CHANNEL;
  }

  /* create local member */
  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    local_member = sdt_add_member(foreign_channel, local_component);
    /* Don't have MID yet so we will assign this later*/
    if (!local_member) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : failed to allocate local member");
      remove_allocations();
      return FAIL;
    }
    allocations |= ALLOCATED_LOCAL_MEMBER;
  }

  /* Create packet buffer*/
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : failed to get new txbuf");
    remove_allocations();
    return FAIL;
  }
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* TODO: add unicast/multicast support? */
  buffer = marshalU16(buffer, 49 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* buffer = marshalU16(buffer, 43 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG); */
  buffer = marshalU8(buffer, SDT_JOIN);
  buffer = marshalCID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_member->mid);
  buffer = marshalU16(buffer, local_channel->number);

  if (is_reciprocal) {
    if (foreign_channel->number > 0) {
      buffer = marshalU16(buffer, foreign_channel->number); /* Reciprocal Channel */
    } else {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_join : reciprocal channel is zero");
      remove_allocations();
      return FAIL;
    }
  } else {
    buffer = marshalU16(buffer, 0);                                     /* Reciprocal Channel */
  }
  buffer = marshalU32(buffer, local_channel->total_seq);
  buffer = marshalU32(buffer, local_channel->reliable_seq);

  /* TODO: add unicast/multicast support. 0 is UNICAST */
  /* buffer = marshalU8(buffer, 0); */
  buffer = marshal_transport_address(buffer, &local_channel->destination_addr, SDT_ADDR_IPV4);
  buffer = marshal_channel_param_block(buffer);
  buffer = marshalU8(buffer, 255); /* TODO, value? */

  /* JOINS are always sent to foreign compoents adhoc address */
  /* and from a adhoc address                                 */
  rlp_add_pdu(tx_buffer, buf_start, 49, NULL);
  /* rlp_add_pdu(tx_buffer, buf_start, 43, NULL); */

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join : channel %d", local_channel->number);

  /* if we have a valid foreign tx_channel */
  if (is_reciprocal) {
    rlp_send_block(tx_buffer, local_channel->sock, &foreign_channel->source_addr);
  } else {
    rlp_send_block(tx_buffer, local_channel->sock, &foreign_component->adhoc_addr);
  }
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send JOIN_ACCEPT
    local_member      - local member
    local_component   - local component
    foreign_component - foreign component

    foreign_component must have a tx_channel
*/
static int
sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *tx_buffer;

  LOG_FSTART();

  assert(local_member);
  assert(local_component);
  assert(foreign_component);

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_join_accept : foreign component without channel");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join_accept : failed to get new txbuf");
    return FAIL;
  }
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer = marshalU16(buffer, 29 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer = marshalU8(buffer, SDT_JOIN_ACCEPT);
  /* data */
  buffer = marshalCID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_component->tx_channel->number);
  buffer = marshalU16(buffer, local_member->mid);
  buffer = marshalU32(buffer, foreign_component->tx_channel->reliable_seq);
  buffer = marshalU16(buffer, local_component->tx_channel->number);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 29, NULL);
  /* JOIN_ACCEPT are sent address where the JOIN came from */
  /* this should be the foreign componets adhoc address    */
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join_accept : channel %d", foreign_component->tx_channel->number);
  rlp_send_block(tx_buffer, sdt_adhoc_socket, &foreign_component->tx_channel->source_addr);
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send JOIN_REFUSE
    foreign_cid         - cid of component we are sendign this to
    local_component     - component sending
    source_addr         - address join message originated from
    foreign_channel_num - channel number we are refusing
    join                - pointer to "data" of original JOIN message
    reason              - reason we are refusing to join
*/
static int
sdt_tx_join_refuse(const cid_t foreign_cid, component_t *local_component, const netx_addr_t *source_addr,
                   uint16_t foreign_channel_num, uint16_t local_mid, uint32_t foreign_rel_seq, uint8_t reason)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *tx_buffer;

  LOG_FSTART();

  assert(local_component);
  assert(source_addr);

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join_refuse : failed to get new txbuf");
    return FAIL;
  }
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer = marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer = marshalU8(buffer, SDT_JOIN_REFUSE);
  /* data */
  buffer = marshalCID(buffer, foreign_cid);
  buffer = marshalU16(buffer, foreign_channel_num); /* leader's channel # */
  buffer = marshalU16(buffer, local_mid);           /* mid that the leader was assigning to me */
  buffer = marshalU32(buffer, foreign_rel_seq);     /* leader's channel rel seq # */
  buffer = marshalU8(buffer, reason);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 28, NULL);

  /* send via our ad-hoc to address it came from */
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join_refuse : channel %d", foreign_channel_num);
  rlp_send_block(tx_buffer, sdt_adhoc_socket, source_addr);
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send LEAVING
    foreign_component   - component receiving
    local_component     - component sending
    local_member        - local member in foreign channel
    reason              - reason we are leaving
*/
static int
sdt_tx_leaving(component_t *foreign_component, component_t *local_component, sdt_member_t *local_member, uint8_t reason)
{
  uint8_t       *buf_start;
  uint8_t       *buffer;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_leaving : foreign component without channel");
    return FAIL;
  }

  foreign_channel = foreign_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_leaving : failed to get new txbuf");
    return FAIL;
  }
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  buffer = marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer = marshalU8(buffer, SDT_LEAVING);
  buffer = marshalCID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_channel->number);
  buffer = marshalU16(buffer, local_member->mid);
  buffer = marshalU32(buffer, foreign_channel->reliable_seq);
  buffer = marshalU8(buffer, reason);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 28, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_leaving : channel %d", foreign_channel->number);

  /* send via channel */
  rlp_send_block(tx_buffer, sdt_adhoc_socket, &foreign_channel->source_addr);
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send NAK
    foreign_component   - component receiving
    local_component     - component sending
    last_missed         - last missed sequence number (we assume the first from channel)
*/
static int
sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed)
{
  uint8_t       *buf_start;
  uint8_t       *buffer;
  rlp_txbuf_t   *tx_buffer;
  sdt_member_t  *local_member;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_nak : foreign component without channel");
    return FAIL;
  }

  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_nak : failed to find local_member");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_nak : failed to get new txbuf");
    return FAIL;
  }
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* FIXME -  add proper nak storm suppression? */
  buffer = marshalU16(buffer, 35 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer = marshalU8(buffer, SDT_NAK);
  buffer = marshalCID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_channel->number);
  buffer = marshalU16(buffer, local_member->mid);
  buffer = marshalU32(buffer, foreign_channel->reliable_seq);    /* last known good one */
  buffer = marshalU32(buffer, foreign_channel->reliable_seq + 1);  /* then the next must be the first I missed */
  buffer = marshalU32(buffer, last_missed);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 35, NULL);

  /* send via channel */
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_nak : channel %d", foreign_channel->number);
  if (local_member->nak & NAK_OUTBOUND) {
    rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  } else {
    rlp_send_block(tx_buffer, sdt_adhoc_socket, &foreign_channel->source_addr);
  }
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/* TODO:         sdt_tx_get_sessions() */

/*****************************************************************************/
/* TODO:         sdt_tx_sessions() */

/*****************************************************************************/
/*
  Received JOIN message
    foreign_cid    - cid of component that sent the message
    source_addr    - address JOIN message was received from
    join           - points to DATA section of JOIN message
    data_len       - length of DATA section.
*/

static void
sdt_rx_join(const cid_t foreign_cid, const netx_addr_t *source_addr, const uint8_t *join, uint32_t data_len)
{
  cid_t           local_cid;
  const uint8_t  *joinp;
  uint16_t        foreign_channel_number;
  uint16_t        local_channel_number;

  uint16_t         local_mid;
  component_t     *local_component = NULL;
  component_t     *foreign_component = NULL;

  sdt_channel_t   *foreign_channel = NULL;
  sdt_channel_t   *local_channel = NULL;
  sdt_member_t    *local_member = NULL;
  sdt_member_t    *foreign_member = NULL;

  uint32_t         foreign_total_seq;
  uint32_t         foreign_reliable_seq;

  uint8_t          address_type;
  uint8_t          adhoc_expiry;
  groupaddr_t      groupaddr;
  int              allocations = 0;


  LOG_FSTART();

/*  auto void remove_allocations(void); */
/*  void remove_allocations(void) { */
#undef remove_allocations
#define remove_allocations() \
    if (allocations & ALLOCATED_LOCAL_MEMBER) sdt_del_member(foreign_channel, local_member); \
    if (allocations & ALLOCATED_FOREIGN_LISTENER) rlp_del_listener(sdt_multicast_socket, foreign_channel->listener); \
    if (allocations & ALLOCATED_FOREIGN_CHANNEL) sdt_del_channel(foreign_component); \
    if (allocations & ALLOCATED_FOREIGN_COMP) sdt_del_component(foreign_component); \
    if (allocations & ALLOCATED_LOCAL_CHANNEL) sdt_del_channel(local_component);
/*  } */

  /* verify data length */
  if (data_len < 40) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: pdu too short");
    return;
  }

  /* get local pointer */
  joinp = join; /* points to data section of join message which starts with dest CID */

  /* get destination cid out of the message */
  unmarshalCID(joinp, local_cid);
  joinp += CIDSIZE;

  /* see if this the dest CID matches one of our local components */
  local_component = sdt_find_comp_by_cid(local_cid);
  if (!local_component) {
    acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_join: Not addressed to me");
    return; /* not addressed to any local component */
  }

  /* Member ID from leader */
  local_mid = unmarshalU16(joinp);
  joinp += sizeof(uint16_t);

  /* Leaders channel number */
  foreign_channel_number = unmarshalU16(joinp);
  joinp += sizeof(uint16_t);

  /* Reciprocal channel number*/
  local_channel_number = unmarshalU16(joinp);
  joinp += sizeof(uint16_t);

  /* Total Sequence Number */
  foreign_total_seq = unmarshalU32(joinp);
  joinp += sizeof(uint32_t);

  /* Reliable Sequence Number */
  foreign_reliable_seq = unmarshalU32(joinp);
  joinp += sizeof(uint32_t);

  local_channel = local_component->tx_channel; /* get pointer to local chan struct in our component */

  /* if we were given a reciprocal chan num in the rx message, then verify it is correct */
  if (local_channel_number) {
    if ((!local_channel) || (local_channel->number != local_channel_number)) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: reciprocal channel number does not match");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_NO_RECIPROCAL);
      remove_allocations();
      return;
    }
  }

  /* if we don't have a tx channel for our CID, create one */
  if (!local_channel) {
    /* create a channel with a random channel number and put its address into our component */
    /* returns the address of the new channel structure */
    local_channel = sdt_add_channel(local_component, rand());
    if (!local_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join : failed to add local channel");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_LOCAL_CHANNEL;
    /* assign mcast port and the next mcast IP address into the channel struct */
    netx_PORT(&local_channel->destination_addr) = htons(SDT_MULTICAST_PORT);
    netx_INADDR(&local_channel->destination_addr) = mcast_alloc_new(local_component);
  }

  /* we must have a local socket struct associated with our channel struct */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_rx_join : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /*  Are we already tracking the src component, if not add it */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component) {
    foreign_component = sdt_add_component(foreign_cid, NULL, false, accUNKNOWN, cbJOIN);
    if (!foreign_component) { /* allocation failure */
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join: failed to add foreign component");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_FOREIGN_COMP;
  }

  /* create foreign channel */
  foreign_channel = foreign_component->tx_channel;
  if (foreign_channel) {
    /* we only can have one channel so it better be the same */
    if ((foreign_channel->number != 0) && (foreign_channel->number != foreign_channel_number)) {
      acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join: multiple channel rejected: %d", foreign_channel_number);
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join: pre-existing foreign channel %d", foreign_channel_number);
  } else {
    /* create a channel with foreign port number and put its address into foreign component  */ /* returns the address of the new channel structure */
    foreign_channel = sdt_add_channel(foreign_component, foreign_channel_number);
    if (!foreign_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join: failed to add foreign channel");
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_FOREIGN_CHANNEL;
  }
  /* add or update sequence numbers */
  foreign_channel->total_seq = foreign_total_seq;
  foreign_channel->reliable_seq = foreign_reliable_seq;
  foreign_channel->source_addr = *source_addr;

  /* TODO: we need to init foreign_channel->sock to the address of a sockets_tbl[] element with 5568 */
  foreign_channel->sock = sdt_multicast_socket;

  joinp = unmarshal_transport_address(joinp, source_addr, &foreign_channel->destination_addr, &address_type); /* get dest port and IP out of rx join msg */

  if (address_type == SDT_ADDR_IPV6) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: Unsupported downstream Address Type");
    sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_BAD_ADDR);
    remove_allocations();
    return;
  }

  if (address_type == SDT_ADDR_IPV4) {
    groupaddr = netx_INADDR(&foreign_channel->destination_addr);
    if (is_multicast(groupaddr)) {
      foreign_channel->listener = rlp_add_listener(sdt_multicast_socket, groupaddr, SDT_PROTOCOL_ID, sdt_rx_handler, foreign_component);
      if (!foreign_channel->listener) {
        acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join: failed to add multicast listener");
        sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
        remove_allocations();
        return;
      }
      allocations |= ALLOCATED_FOREIGN_LISTENER;
    } else {
      remove_allocations();
      return;
    }
  }

  /* local member is our local component in foreign member list */
  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (local_member) { /* if local member does not exist */
    /* verify mid */
    if (foreign_channel->number != 0 && local_member->mid != local_mid) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: have local member but MID does not match");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_ALREADY_MEMBER);
      remove_allocations();
      return;
    }
  } else {
    /* add local component to foreign channel */
    local_member = sdt_add_member(foreign_channel, local_component);
    if (!local_member) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join: failed to add local member");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_LOCAL_MEMBER;
    /* set move flag up one state */
  }

  /* set mid */
  local_member->mid = local_mid;
  /* fill in the member structure */
  joinp = unmarshal_channel_param_block(local_member, joinp);
  adhoc_expiry = unmarshalU8(joinp);
  joinp += sizeof(uint8_t);

  /* set time out to check if leader goes away*/
  local_member->expires_ms = local_member->expiry_time_s * 1000;
  /* acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join : local_member expire_ms %d", local_member->expires_ms); */

  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    foreign_member = sdt_add_member(local_channel, foreign_component);
    if (!foreign_member) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join : failed to add foreign member");
    }
  }
  foreign_member->mak_retries = MAK_MAX_RETRIES;

  foreign_channel->number = foreign_channel_number;

  /* send the join accept */
  if (sdt_tx_join_accept(local_member, local_component, foreign_component) == FAIL) {
    remove_allocations();
    return;
  }

  /* if we were sent a local channel number then it is a reciprocal join */
  if (local_channel_number) {
    /* verifiy states anyway */
    if (foreign_member->state == msWAIT_FOR_ACK && local_member->state == msEMPTY) {
      if (sdt_tx_ack(local_component, foreign_component) == FAIL) {
        remove_allocations();
        return;
      }
      local_member->state = msWAIT_FOR_ACK;
    } else {
      remove_allocations();
      return;
    }
  } else {
    if (foreign_member->state == msEMPTY && local_member->state == msEMPTY) {
      if (sdt_tx_join(local_component, foreign_component, true) == FAIL) {
        remove_allocations();
        return;
      }
      local_member->state = msWAIT_FOR_ACK;
      foreign_member->state = msWAIT_FOR_ACCEPT;
    } else {
      remove_allocations();
      return;
    }
  }
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join : channel %d, mid %d", foreign_channel->number, local_mid);

  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Receive JOIN_ACCEPT
    foreign_cid    - source of component that sent the JOIN_ACCEPT
    join_accept    - points to DATA section of JOIN_ACCEPT message
    data_len       - length of DATA section.
*/
static void
sdt_rx_join_accept(const cid_t foreign_cid, const uint8_t *join_accept, uint32_t data_len)
{
  component_t     *local_component;
  component_t     *foreign_component;
  sdt_channel_t   *local_channel;
  sdt_member_t    *local_member;
  sdt_channel_t   *foreign_channel;
  sdt_member_t    *foreign_member;

  uint16_t         foreign_channel_number;
  uint16_t         foreign_mid;
  cid_t            local_cid;
  uint16_t         local_channel_number;
  uint32_t         rel_seq_number;

  LOG_FSTART();

  /* verify data length */
  if (data_len != 26) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: foreign_component not found");
    return;
  }

  /* verify leader CID */
  unmarshalCID(join_accept, local_cid);
  join_accept += CIDSIZE;
  local_component = sdt_find_comp_by_cid(local_cid);
  if (!local_component) {
    acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_join_accept: not for me");
    return;
  }

  local_channel = local_component->tx_channel;
  /* verify we have a channel */
  if (!local_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept : local component without channel");
    return;
  }

  /* verify we have a matching channel number */
  local_channel_number = unmarshalU16(join_accept);
  join_accept += sizeof(uint16_t);
  if (local_component->tx_channel->number != local_channel_number) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: local channel %d not found", local_channel_number);
    return;
  }

  /* this is the MID of that the leader assigned when sending the JOIN*/
  foreign_mid = unmarshalU16(join_accept);
  join_accept += sizeof(uint16_t);

  /* TODO: do something with this?? */
  rel_seq_number = unmarshalU32(join_accept);
  join_accept += sizeof(uint32_t);

  /* reciprocal channel*/
  foreign_channel_number = unmarshalU16(join_accept);
  join_accept += sizeof(uint16_t);

  /* should have a foreign member in our local channel by now */
  foreign_member = sdt_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: foreign member not found");
    return;
  }

  /* if foreign component has a channel, it should match the one sent*/
  foreign_channel = foreign_component->tx_channel;
  if (!foreign_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: local member not found");
    return;
  }

  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: local member not found");
    return;
  }

  /* state we should be in after we initiated a session */
  if (foreign_member->state == msWAIT_FOR_ACCEPT && local_member->state == msEMPTY) {
    foreign_member->state = msWAIT_FOR_ACK;
  }

  if (foreign_member->state == msWAIT_FOR_ACCEPT && local_member->state == msWAIT_FOR_ACK) {
    if ((foreign_channel->number > 0) && (foreign_channel->number != foreign_channel_number)) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: reciprocal channel mismatch");
      return;
    }
    /* send ACK */
    if (sdt_tx_ack(local_component, foreign_component) == FAIL) {
      return;
    }
    foreign_member->state = msWAIT_FOR_ACK;
  }

  /* set foreign epiry */
  foreign_member->expiry_time_s = FOREIGN_MEMBER_EXPIRY_TIME_ms/1000;
  foreign_member->expires_ms = FOREIGN_MEMBER_EXPIRY_TIME_ms;


  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join_accept : channel %d, mid %d", local_channel_number, foreign_mid);

  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Receive JOIN_REFUSE
    foreign_cid    - source of component that sent the JOIN_REFUSE
    join_refuse    - points to DATA section of JOIN_REFUSE message
    data_len       - length of DATA section.
*/
static void
sdt_rx_join_refuse(const cid_t foreign_cid, const uint8_t *join_refuse, uint32_t data_len)
{
  cid_t             local_cid;
  component_t      *local_component;
  uint16_t          local_channel_number;
  component_t      *foreign_component;
  uint16_t          foreign_mid;
  uint32_t          rel_seq_num;
  uint8_t           reason_code;
  sdt_member_t     *foreign_member;

  LOG_FSTART();

  /* verify data length */
  if (data_len != 25) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse: foreign_component not found");
    return;
  }

  /* get leader */
  unmarshalCID(join_refuse, local_cid);
  join_refuse += CIDSIZE;
  local_component = sdt_find_comp_by_cid(local_cid);
  if (!local_component) {
    acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_join_refuse: Not for me");
    return;
  }

  /* verify we have a channel */
  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse : local component without channel");
    return;
  }

  /* get channel */
  local_channel_number = unmarshalU16(join_refuse);
  join_refuse += sizeof(uint16_t);

  /* get member */
  foreign_mid = unmarshalU16(join_refuse);
  join_refuse += sizeof(uint16_t);

  /* get reliable sequence number */
  rel_seq_num = unmarshalU32(join_refuse);
  join_refuse += sizeof(uint32_t);

  /* get reason */
  reason_code = unmarshalU8(join_refuse);
  join_refuse += sizeof(uint8_t);

  /* well, if they don't want to play, nuke-em */
  /* sdt_rx_join_refuse comes from member so he is our list */
  foreign_member = sdt_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (foreign_member) {
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join_refuse: channel %d, mid, %d, reason %d", local_channel_number, foreign_mid, reason_code);
    /* update state */
    /* and remove it on next tick*/
    foreign_member->state = msDELETE;
  } else {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse: foreign member not found");
  }
  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Receive LEAVING
    foreign_cid    - source of component that send the LEAVING message
    leaving        - points to DATA section of LEAVING message
    data_len       - length of DATA section.
*/
static void
sdt_rx_leaving(const cid_t foreign_cid, const uint8_t *leaving, uint32_t data_len)
{
  component_t     *foreign_component;
  component_t     *local_component;
  cid_t            local_cid;
  uint16_t         local_channel_number;
  uint16_t         foreign_mid;
  sdt_member_t    *foreign_member;

  LOG_FSTART();

  /* verify data length */
  if (data_len != 25) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: foreign_component not found");
    return;
  }

  /* get leader cid */
  unmarshalCID(leaving, local_cid);
  leaving += CIDSIZE;
  local_component = sdt_find_comp_by_cid(local_cid);
  if (!local_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: local_component not found");
    return;
  }

  /* get channel number */
  local_channel_number = unmarshalU16(leaving);
  leaving += sizeof(uint16_t);
  if (!local_component->tx_channel) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: local component without channel");
      return;
  }
  if (local_component->tx_channel->number != local_channel_number) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: local channel number mismatch");
    return;
  }

  foreign_mid = unmarshalU16(leaving);
  leaving += sizeof(uint16_t);


  /* leaving comes from a member so he is in our list */
  foreign_member = sdt_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (foreign_member)  {
    if (foreign_member->state == msCONNECTED) {
      if (local_component->callback)
        (*local_component->callback)(SDT_EVENT_DISCONNECT, local_component, foreign_component, false, NULL, 0, NULL);
    }

    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_leaving : channel %d, mid %d", local_channel_number, foreign_mid);
    /* update state */
    /* and remove it on next tick*/
    foreign_member->state = msDELETE;

  } else {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: foreign member not found");
  }

  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Receive NAK
    foreign_cid    - source of component sending NAK
    nak            - points to DATA section of NAK message
    data_len       - length of DATA section.
*/
static void
sdt_rx_nak(const cid_t foreign_cid, const uint8_t *nak, uint32_t data_len)
{
  cid_t            local_cid;
  component_t     *local_component;
  uint16_t         local_channel_number;
  component_t     *foreign_component;

  uint32_t        first_missed;
  uint32_t        last_missed;

  LOG_FSTART();

  /* verify data length */
  if (data_len != 32) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: foreign_component not found");
    return;
  }

  /* get Leader CID */
  unmarshalCID(nak, local_cid);
  nak += CIDSIZE;
  local_component = sdt_find_comp_by_cid(local_cid);
  if (!local_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: local_component not found");
    return;
  }

  local_channel_number = unmarshalU16(nak);
  nak += sizeof(uint16_t);
  if (!local_component->tx_channel) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: local component without channel");
      return;
  }
  if (local_component->tx_channel->number != local_channel_number) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: local channel number mismatch");
    return;
  }

  /* The next field is the MID - this is not used - throw away */
  nak += sizeof(uint16_t);

  /* The next field is the member's Ack point - this is not used - throw away */
  nak += sizeof(uint32_t);

  first_missed = unmarshalU32(nak);
  nak += sizeof(uint32_t);

/*  num_back = local_component->tx_channel->reliable_seq - first_missed; */
  if (first_missed < local_component->tx_channel->oldest_avail)  {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: Requested Wrappers not avail");
    return;
  }

  last_missed = unmarshalU32(nak);
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_nak : channel %" PRId16 ", %" PRId32 " - %" PRId32 , local_channel_number, first_missed, last_missed);

  if (last_missed < first_missed) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: Illogical parameters");
    return;
  }

  /* resend data*/
  nak += sizeof(uint32_t);
  while(first_missed <= last_missed) {
    if (sdt_flag_for_resend(local_component->tx_channel, first_missed) == FAIL) {
      /* send mak to component to fresh what it knows about our oldest wrapper avail */
      sdt_tx_mak_one(local_component, foreign_component);
    }
    first_missed++;
  }
  LOG_FEND();
}

/*****************************************************************************/
/*  Mark buffer for resend
    channel: channel to match on which is was sent
    reliable_seq: sequence number to resend

    returns -1 if fails, 0 otherwise

    NOTE: There is a flaw in this logic. Since we delay the sending till
    later, it is possible that our buffer gets pushed out as new messages come in
    and the flagged buffer will get deleted.
 */
static int
sdt_flag_for_resend(sdt_channel_t *channel, uint32_t reliable_seq)
{
  sdt_channel_t  *tx_channel;
  sdt_resend_t   *resend;

  LOG_FSTART();
  resend = resends;

  while (resend) {
    tx_channel = resend->local_component->tx_channel;
    if (tx_channel) {
      if ((resend->reliable_seq == reliable_seq) & (tx_channel == channel)) {
        /* set timer to trigger resending */
        resend->blanktime_ms = NAK_BLANKTIME_ms;
        LOG_FEND();
        return OK;
      }
    }
  }
  acnlog(LOG_WARNING | LOG_SDT, "sdt_flag_for_resend: sequence %" PRId32 " not found", reliable_seq);
  return FAIL;
}

/*****************************************************************************/
/*
 * Resend a buffer
      returns -1 if fails, 0 otherwise
 */
static int
sdt_tx_resend(sdt_resend_t *resend)
{
  rlp_txbuf_t    *tx_buffer;
  sdt_channel_t  *tx_channel;
  uint8_t        *pdup;

  LOG_FSTART();
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_resend : HACK");
  return OK;



  tx_channel = resend->local_component->tx_channel;
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, resend->local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_resend : failed to get new txbuf");
    return FAIL;
  }
  /* HACK FOR NOW to re-adjust oldest wrapper */
  *(uint32_t*)(resend->pdup + 13) = tx_channel->oldest_avail;

  pdup = rlp_init_block(tx_buffer, NULL);
  pdup = rlp_add_pdu(tx_buffer, resend->pdup, resend->size, NULL);
  rlp_send_block(tx_buffer, tx_channel->sock, &tx_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_resend : resent");
  LOG_FEND();
  return OK;
}


/*****************************************************************************/
/*
  Receive a REL_WRAPPER or UNREL_WRAPPER packet
    foreign_cid    - cid of component that sent the wrapper
    source_addr    - address wrapper came from
    is_reliable    - flag indicating if type of wrapper
    wrapper        - pointer to wrapper DATA block
    data_len       - length of data block
    ref            - refernce pointer (LWIP pbuf)
*/
static void
sdt_rx_wrapper(const cid_t foreign_cid, const netx_addr_t *source_addr, const uint8_t *wrapper,  bool is_reliable, uint32_t data_len, void *ref)
{
  component_t     *foreign_component;
  sdt_channel_t   *foreign_channel;
  sdt_member_t    *local_member;
/*  sdt_member_t    *foreign_member; */

  const uint8_t    *wrapperp;
  const uint8_t    *data_end;
  uint32_t          data_size = 0;
  const uint8_t    *pdup, *datap = NULL;

  uint16_t    local_mid = 0;
  uint32_t    protocol = 0;
  uint16_t    association = 0;

  uint16_t channel_number;
  uint32_t total_seq;
  uint32_t reliable_seq;
  uint32_t oldest_avail;
  uint16_t first_mid;
  uint16_t last_mid;
  uint16_t mak_threshold;

  int      found_member;

  UNUSED_ARG(source_addr);

  LOG_FSTART();

  /* verify length */
  if (data_len < 20) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: Invalid data_len");
    return;
  }
  data_end = wrapper + data_len;


  /* our callback saved the reference to the component */
  foreign_component = (component_t*)ref;
  if (!cidIsEqual(foreign_component->cid, foreign_cid)) {
    /* just return, not for us */
    return;
  }

#if 0
  /* verify we are tracking this component */
  foreign_component = sdt_find_comp_by_cid(foreign_cid);
  if (!foreign_component)  {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: Not tracking this component");
    return;
  }
#endif

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper : foreign component without channel");
    return;
  }

  foreign_channel = foreign_component->tx_channel;

  /* local pointer */
  wrapperp = wrapper;

  /* channel message came in on */
  channel_number = unmarshalU16(wrapperp);
  wrapperp += sizeof(uint16_t);

  /* Total Sequence Number */
  total_seq = unmarshalU32(wrapperp);
  wrapperp += sizeof(uint32_t);

  /* Reliable Sequence Number */
  reliable_seq = unmarshalU32(wrapperp);
  wrapperp += sizeof(uint32_t);

  /* Oldest available seq number */
  oldest_avail = unmarshalU32(wrapperp);
  wrapperp += sizeof(uint32_t);

  /* First MID to ACK */
  first_mid = unmarshalU16(wrapperp);
  wrapperp += sizeof(uint16_t);

  /* Last MID to ACK */
  last_mid = unmarshalU16(wrapperp);
  wrapperp += sizeof(uint16_t);

  /* MAK threshold */
  mak_threshold = unmarshalU16(wrapperp);
  wrapperp += sizeof(uint16_t);

  /* check the sequencing */
  switch(check_sequence(foreign_channel, is_reliable, total_seq, reliable_seq, oldest_avail)) {
    case SEQ_VALID :
      acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_wrapper: Sequence correct");
      break; /* continue on with this packet */
    case SEQ_NAK :
      /* initiate nak processing */
      acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_wrapper: Missing wrapper(s) detected - Sending NAK");
      local_member = foreign_channel->member_list;
      while (local_member) {
        /* we currently do not provide retries on getting lost packets so we
         * just send this once an hope it works */
        /* TODO: We need to deal with NAK hold off, retries and timeouts. */
        sdt_tx_nak(foreign_component, local_member->component, reliable_seq);
        local_member = local_member->next;
      }
      /* in the odd case where an unreliable packet lets us know we missed a packet, we
       * still need to process the unreliable packet */
      if (is_reliable) return;
      break;
    case SEQ_OLD_PACKET :
      acnlog(LOG_INFO | LOG_SDT, "sdt_rx_wrapper: Old (Out of sequence) wrapper detected - Discarding");
      /* Discard */
      return;
      break;
    case SEQ_DUPLICATE :
      acnlog(LOG_INFO | LOG_SDT, "sdt_rx_wrapper: Duplicate wrapper detected - Discarding");
      /* Discard */
      return;
      break;
    case SEQ_LOST :
      acnlog(LOG_INFO | LOG_SDT, "sdt_rx_wrapper: Lost Reliable Wrappers not available - LOST SEQUENCE");
      /* Discard and send sequence lost message to leader */
      local_member = foreign_channel->member_list;
      while (local_member) {
        /* send "I'm out of here" and delete the member */
        /* TODO: DMP/application notification */
        sdt_tx_leaving(foreign_component, local_member->component, local_member, SDT_REASON_LOST_SEQUENCE);
        /* remove on next tick */
        local_member->state = msDELETE;
        local_member = local_member->next;
      }
      return;
      break;
  }

/* ************************** */

  /* TODO: Currently doesn't care about the MAK Threshold */

  /* send acks if needed */
  local_member = foreign_channel->member_list;
  while (local_member) {
    if (((local_member->state == msJOINED) || (local_member->state == msCONNECTED)) && ((local_member->mid >= first_mid) && (local_member->mid <= last_mid)))  {
      /* send ack */
      sdt_tx_ack(local_member->component, foreign_component);
      /* a MAK is "data on the channel" so we can reset the expiry timeout */
      /* this can get set again based on the wrapped packet MID */
    }
    /* Got data so reset timer */
    local_member->expires_ms = local_member->expiry_time_s * 1000;
    /* acnlog(LOG_DEBUG | LOG_SDT,"sdt_rx_wrapper: local_member expires_ms %d", local_member->expires_ms); */

    local_member = local_member->next;
  }

  /* start of our client block containing one or multiple pdus*/
  pdup = wrapperp;

  /* check to see if we have any packets */
  if (pdup == data_end) {
    acnlog(LOG_DEBUG | LOG_SDT,"sdt_rx_wrapper: wrapper with no pdu");
    return;
  }

  /* check first packet */
  if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_wrapper: illegal first PDU flags");
    return;
  }

  /* decode and dispatch the wrapped pdus (SDT Client Block, wrapped SDT or DMP) */
  while(pdup < data_end)  {
    const uint8_t *pp;
    uint8_t  flags;

    flags = unmarshalU8(pdup);

    /* save pointer, on second pass, we don't know if this is vector, header or data until we look at flags*/
    pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);
    /* fail if outside our packet */
    if (pdup > data_end) {
      acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_wrapper: packet length error");
      return;
    }

    /* Get vector or leave the same as last = MID*/
    if (flags & VECTOR_bFLAG) {
      local_mid = unmarshalU16(pp);
      pp += sizeof(uint16_t);
    }

    /* Get header or leave the same as last = Client Protocal & Association*/
    if (flags & HEADER_bFLAG) {
      protocol = unmarshalU32(pp);
      pp += sizeof(uint32_t);
      association = unmarshalU16(pp);
      pp += sizeof(uint16_t);
    }
    /* pp now points to potential start of data*/

    /* get packet data (or leave pointer to old data) */
    if (flags & DATA_bFLAG) {
      datap = pp;
      /* calculate reconstructed pdu length */
      data_size = pdup - pp;
    }

    /* now dispatch to members */
    found_member = 0;
    local_member = foreign_channel->member_list;
    while (local_member) {
      /* either all or to one mid */
      if ((local_mid == 0xFFF) || (local_member->mid == local_mid)) {
        found_member = 1;
        if (association) {
          /* the association channel is the local channel */
          if (local_member->component->tx_channel->number != association) {
            acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: association channel not correct");
            return;
          }

           /* TODO: WRF - what it this below and why is it commented out? */
/*          else { */
            /* got data so reset timer */
/*            local_member->expires_ms = local_member->expiry_time_s * 1000; */
            /* now find the foreign component in the local channel and mark it as having been acked */
/*            foreign_member = sdt_find_member_by_component(local_member->component->tx_channel, foreign_component); */
/*            if (foreign_member) { */
/*              foreign_member->mak_ms = 1000; */ /*TODO: un-hard code this */
/*            } else { */
/*              acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: foreign member not found"); */
/*            } */
/*          } */
        }

        /*  TODO: WRF - what it this below and why is it commented out? */
        /* we have data on this channel from our leader so reset the expiry timeout */
        /* local_member->expires_ms = local_member->expiry_time_s * 1000; */
        switch (protocol) {
          case SDT_PROTOCOL_ID:
            sdt_client_rx_handler(local_member->component, foreign_component, datap, data_size);
            break;
          #if CONFIG_DMP
          case DMP_PROTOCOL_ID:
            acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: DMP_PROTOCOL_ID");
/*
            if (local_member->component->callback)
               (*local_member->component->callback)(SDT_EVENT_DATA, local_member->component, foreign_component, is_reliable, datap, data_size, ref);
*/
            /* WRF - below is old stub to be deleted when above is tested */
            dmp_client_rx_handler(local_member->component, foreign_component, is_reliable, datap, data_size, ref);
            break;
          #endif
          default:
            acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: unknown protocol");
        }
      }

      local_member = local_member->next;
    }
    if (!found_member) {
      acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_wrapper: no matching MID");
    }
  }

  LOG_FEND();
  return;
}


/*****************************************************************************/
/* TODO:         sdt_rx_get_sessions() */

/*****************************************************************************/
/* TODO:         sdt_rx_sessions() */

/*****************************************************************************/
/*
  Send ACK
    local_component   -
    foreign_component -

  ACK is sent in a reliable wrapper
*/
static int
sdt_tx_ack(component_t *local_component, component_t *foreign_component)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  /* this one fails sometimes */
  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_ack : local component without channel");
    return FAIL;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_ack : foreign component without channel");
    return FAIL;
  }

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get foreign member in local channel */
  /* this will be used to get the MID to send it to */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_ack : failed to get foreign_member");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_ack : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_ack : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_UNRELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, foreign_channel->number);

  /* add datagram (ACK message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_ACK);
  datagram = marshalU32(datagram, foreign_channel->reliable_seq);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram - wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram - wrapper, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_ack : channel %d", foreign_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/* TODO:         sdt_tx_channel_params(); */

/*****************************************************************************/
/*
  Send LEAVE
    local_component   - local component sending LEAVE
    foreign_component - foreign component receiving
    foreign_member    - foreign member in local channel

  LEAVE is sent in a reliable wrapper
*/
static int
sdt_tx_leave(component_t *local_component, component_t *foreign_component, sdt_member_t *foreign_member)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);
  assert(foreign_member);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_leave : local component without channel");
    return FAIL;
  }

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_leave : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_leave : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram (LEAVE message) */
  datagram = marshalU16(datagram, 3 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_LEAVE);

  /* add length to client block pdu */
  marshalU16(client_block, (10+3) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (datagram - wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_leave : channel %d", local_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send CONNECT
    local_component   - local component sending CONNECT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  CONNECT is sent in a reliable wrapper
*/
/* TODO: We need timeout on connect */

static int
sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect : local component without channel");
    return FAIL;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect : foreign component without channel");
    return FAIL;
  }

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect : failed to get foreign_member");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_connect : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_connect : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram CONNECT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_CONNECT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (pdu_size) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);
  acnlog(LOG_INFO | LOG_SDT, "sdt_tx_connect : released txbuf");

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send CONNECT_ACCEPT
    local_component   - local component sending CONNECT_ACCEPT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  CONNECT_ACCEPT is sent in a reliable wrapper
*/
static int
sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect_accept : local component without channel");
    return FAIL;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect_accept : foreign component without channel");
    return FAIL;
  }

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect_accept : failed to get foreign_member");
    return FAIL;
  }

  /* Create packet buffer */
  /* tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component); */
  /* TODO: I had passed componenet to this for some reason.... */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_connect_accept : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_connect_accept : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, foreign_channel->number);

  /* add datagram (CONNECT_ACCEPT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_CONNECT_ACCEPT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (pdu_size) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/* TODO:         sdt_tx_connect_refuse(); */

/*****************************************************************************/
/*
  Send DISCONNECT
    local_component   - local component sending DISCONNECT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  DISCONNECT is sent in a reliable wrapper
*/
static int
sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : local component without channel");
    return FAIL;
  }

  if (!local_component->tx_channel->sock) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : local component without socket");
    return FAIL;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : foreign component without channel");
    return FAIL;
  }


  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : failed to get foreign_member");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_disconnect : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram (DISCONNECT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_DISCONNECT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (pdu_size) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/* TODO:         sdt_tx_disconecting(); */
/*
  Send DISCONNECTING
    local_component   - local component sending DISCONNECTING
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  DISCONNECTING is sent in a reliable wrapper
*/
static int
sdt_tx_disconnecting(component_t *local_component, component_t *foreign_component, uint32_t protocol, uint8_t reason)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnecting : local component without channel");
    return FAIL;
  }

  if (!local_component->tx_channel->sock) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnecting : local component without socket");
    return FAIL;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnecting : foreign component without channel");
    return FAIL;
  }


  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : failed to get foreign_member");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_disconnect : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, SDT_PROTOCOL_ID, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram (DISCONNECT message) */
  datagram = marshalU16(datagram, 8 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_DISCONNECTING);
  datagram = marshalU32(datagram, protocol);
  datagram = marshalU8(datagram, reason);

  /* add length to client block pdu */
  marshalU16(client_block, (10+8) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (pdu_size) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}


/*****************************************************************************/
/*
  Send MAK
    local_component   - local component sending MAK

  This is a psudo command as it just sends an empty wrapper with MAK flags set
*/
static int
sdt_tx_mak_all(component_t *local_component)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);

  /* just to make it easier */
  local_channel = local_component->tx_channel;

  if (!local_channel) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_mak_all : component has no channel");
    return FAIL;
  }


  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_mak_all : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);


  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_mak_all : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_UNRELIABLE, local_channel, 0x0001, 0xffff, 0); /* 0 threshold */

  /* add length to wrapper pdu */
  marshalU16(wrapper, (client_block-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, client_block-wrapper, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_mak_all : channel %d", local_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}

/*****************************************************************************/
/*
  Send MAK to one member
    local_component   - local component sending MAK
    foreign_component  - meber to send MAK to.

  This is a psudo command as it just sends an empty wrapper with MAK flags set
*/
static int
sdt_tx_mak_one(component_t *local_component, component_t *foreign_component)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *local_channel;
  sdt_member_t  *foreign_member;
  uint16_t      mid;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  /* just to make it easier */
  local_channel = local_component->tx_channel;

  if (!local_channel) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_mak_one : component has no channel");
    return FAIL;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_mak_one : failed to get new txbuf");
    return FAIL;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);


  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_mak_one : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_mak_one : comonent not member of channel");
    return FAIL;
  }

  mid = foreign_member->mid;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_UNRELIABLE, local_channel, mid, mid, 0); /* 0 threshold */

  /* add length to wrapper pdu */
  marshalU16(wrapper, (client_block-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, client_block-wrapper, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_mak_one : channel %d", local_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

  LOG_FEND();
  return OK;
}


/*****************************************************************************/
/*
  Send protocol data
    local_component   - local component sending CONNECT
    foreign_component - foreign component receiving     (optional)
    protocol          - DMP we hope!
    data              - pointer to data

  data is sent in a reliable wrapper
*/
void
sdt_tx_reliable_data(component_t *local_component, component_t *foreign_component, uint32_t protocol, bool response, void *data, uint32_t data_len)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member = NULL;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  uint16_t       association;
  uint8_t       *pdup;
  uint16_t       pdu_size;

  LOG_FSTART();

  /* must have these */
  assert(local_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_reliable_data : local component without channel");
    return;
  }

  /* just to make it easier */
  local_channel = local_component->tx_channel;

  /* Assume it is outbound */
  association = CHANNEL_OUTBOUND_TRAFFIC;

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_reliable_data : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* Test foreign componet and set values */
  if (foreign_component) {
    /* get local member in foreign channel */
    foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
    if (!foreign_member) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_reliable_data : failed to get foreign_member");
      return;
    }

    if (!foreign_component->tx_channel) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_reliable_data : foreign component without channel");
      return;
    }
    foreign_channel = foreign_component->tx_channel;

    /* response to foreign component ? */
    if (response) {
      association = foreign_channel->number;
    }
  }

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_reliable_data : failed to get new txbuf");
    return;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  if (foreign_component) {
    datagram = sdt_format_client_block(client_block, foreign_member->mid, protocol, association);
  } else {
    datagram = sdt_format_client_block(client_block, 0, protocol, CHANNEL_OUTBOUND_TRAFFIC);
  }

  /* add datagram message) */
  memcpy(datagram, data, data_len);
  datagram += data_len;

  /* add length to client block pdu */
  marshalU16(client_block, (uint16_t)(10+data_len) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  pdu_size = datagram - wrapper;
  marshalU16(wrapper, (pdu_size) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdu_size, &pdup);

  /* save data for reliable resending if needed */
  sdt_save_buffer(tx_buffer, pdup, pdu_size, local_component);

  /* and send it on */
#if acntestlog(LOG_INFO | LOG_SDT)
  {
  uint32_t relseq = unmarshalU32(pdup + 0x09);
  acnlog(LOG_INFO | LOG_SDT, "sdt_tx_reliable_data : %" PRIu32 ", %" PRIu16, relseq, pdu_size);
  }
#endif
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);
  LOG_FEND();
}

/*****************************************************************************/
/*
  Receive ACK
    local_component   - local component receiving ACK
    foreign_component - foreign component sending
    data              - pointer to data section of ACK
    data_len          - length of data section

    by now, we have verified that the wrapper was to the local component
    and the association matched the local components channel.
*/
static void
sdt_rx_ack(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       reliable_seq;
  sdt_member_t  *foreign_member;
  sdt_member_t  *local_member;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : foreign component without channel");
    return;
  }

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;


  /* verify data length */
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack: Invalid data_len");
    return;
  }

  /* Foreign Member */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : failed to get foreign_member");
    return;
  }

  /* Local member */
  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : failed to get local_member");
    return;
  }

  /* TODO: how should this be processed */
  reliable_seq = unmarshalU32(data);

  /* reset timer */
  foreign_member->expires_ms = -1; /* shuts off timer */

  if (foreign_member->state == msWAIT_FOR_ACK) {
    foreign_member->state = msJOINED;
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_ack : foreign member %d JOINED", foreign_member->mid);
  }

  if (local_member->state == msWAIT_FOR_ACK) {
    local_member->state = msJOINED;
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_ack : local member %d JOINED", local_member->mid);
  }

  if (local_member->state == msJOINED && foreign_member->state == msJOINED) {
    /* TODO: if we don't connect this will keep trying, not a good idea.... */
    #if CONFIG_DMP
    sdt_tx_connect(local_component, foreign_component, DMP_PROTOCOL_ID);
    #endif
  }

  LOG_FEND();
  return;
}

/*****************************************************************************/
/* TODO:         sdt_rx_channel_params(); */

/*****************************************************************************/
/*
  Receive LEAVE
    local_component   - local component receiving LEAVE
    foreign_component - foreign component sending
    data              - pointer to data section of LEAVE
    data_len          - length of data section
*/
static void
sdt_rx_leave(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  sdt_member_t *local_member;
  sdt_member_t *foreign_member;

  UNUSED_ARG(data);

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leave : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leave : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 0) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leave: Invalid data_len");
    return;
  }

  local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
  if (local_member) {
    if (local_member->state == msCONNECTED) {
      if (local_component->callback)
        (*local_component->callback)(SDT_EVENT_DISCONNECT, foreign_component, local_component, false, NULL, 0, NULL);
    }
    /* send "I'm out of here" and delete the member */
    sdt_tx_leaving(foreign_component, local_member->component, local_member , SDT_REASON_ASKED_TO_LEAVE);
    /* update state */
    /* and remove on next tick */
    local_member->state = msDELETE;
  } else {
    acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_leave: no local member");
  }

  foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
  if (foreign_member) {
    local_member->state = msDELETE;
  } else {
    acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_leave: no foreign member");
  }
  LOG_FEND();
}

/*****************************************************************************/
/*
  Receive CONNECT
    local_component   - local component receiving CONNECT
    foreign_component - foreign component sending
    data              - pointer to data section of CONNECT
    data_len          - length of data section
*/
static void
sdt_rx_connect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t      protocol;
  sdt_member_t *local_member;
/*  sdt_member_t *foreign_member; */

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect: Invalid data_len");
    return;
  }

  protocol = unmarshalU32(data);

  /* connect should come from leader so we are member of his channel */
  local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);

  /* my membership in him */
  if (!local_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect: local member not found");
    return;
  }

/*  if (!foreign_member) { */
/*    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect: foreign member not found"); */
/*    return; */
/*  } */

  switch (protocol) {
    #if CONFIG_DMP
    case DMP_PROTOCOL_ID:
      if (sdt_tx_connect_accept(local_component, foreign_component, protocol) == OK) {
        local_member->state = msCONNECTED;
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_connect: session established");
        /* let our app know */
        if (local_component->callback) {
          (*local_component->callback)(SDT_EVENT_CONNECT, foreign_component, local_component, false, NULL, 0, NULL);
        }
      }
      break;
    #endif
    default:
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect: unsupported protocol");
      /* TODO: send refuse */
  }
  LOG_FEND();
}

/*****************************************************************************/
/*
  Receive CONNECT_ACCEPT
    local_component   - local component receiving CONNECT_ACCEPT
    foreign_component - foreign component sending
    data              - pointer to data section of CONNECT_ACCEPT
    data_len          - length of data section
*/
static void
sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       protocol;
  sdt_member_t  *foreign_member;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);

  /* connect_accept should come from member so he is a member of our channel */
  foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
  if (foreign_member) {
    switch (protocol) {
      #if CONFIG_DMP
      case DMP_PROTOCOL_ID:
        foreign_member->state = msCONNECTED;
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_connect_accept: session established");
        /* let our app know */
        if (local_component->callback) {
          (*local_component->callback)(SDT_EVENT_CONNECT, local_component, foreign_component, false, NULL, 0, NULL);
        }
        break;
      #endif
      default:
        acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept: Unknown protcol");
    }
  } else {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept: No foreign member");
  }
  LOG_FEND();
}

/*****************************************************************************/
/* sdt_rx_connect_refuse(); */
static void
sdt_rx_connect_refuse(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       protocol;
  uint8_t        reason;
  sdt_member_t  *foreign_member;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_refuse : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_refuse : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 5) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_refuse: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);
  data += sizeof(uint32_t);

  /* get reason */
  reason = unmarshalU8(data);
  data += sizeof(uint8_t);

  /* connect_refuse should come from member so he is a member of our channel */
  foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
  if (foreign_member) {
    switch (protocol) {
      #if CONFIG_DMP
      case DMP_PROTOCOL_ID:
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_connect_refuse : channel %" PRIu16 ", protocol: %" PRIu32 ", reason %" PRIu8, local_component->tx_channel->number, protocol, reason);
        /* TODO: Callback to DMP/APPLICATION */
        break;
      #endif
      default:
      acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_connect_refuse: Unknown protocol");
    }
  } else {
    acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_connect_refuse: No foreign member");
  }
  LOG_FEND();
}


/*****************************************************************************/
/* sdt_rx_disconnect(); */
static void
sdt_rx_disconnect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       protocol;
  sdt_member_t  *local_member;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnect : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnect : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnect: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);
  data += sizeof(uint32_t);

  /* connect_refuse should come from leader so we are a member of his channel */
  local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
  if (local_member) {
    switch (protocol) {
      #if CONFIG_DMP
      case DMP_PROTOCOL_ID:
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_disconnect : channel %" PRIu16 ", protocol: %" PRIu32, foreign_component->tx_channel->number, protocol);
        if (local_component->callback)
          (*local_component->callback)(SDT_EVENT_DISCONNECT, foreign_component, local_component, false, NULL, 0, NULL);

        sdt_tx_disconnecting(local_component, foreign_component, DMP_PROTOCOL_ID, SDT_REASON_ASKED_TO_LEAVE);
        local_member->state = msJOINED;
        break;
      #endif
      default:
        acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_disconnect: Unknown protocol");
    }
  } else {
    acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_disconnect: No local member");
  }
  LOG_FEND();
}

/*****************************************************************************/
/* TODO:         sdt_rx_disconecting(); */
static void
sdt_rx_disconnecting(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       protocol;
  uint8_t        reason;
  sdt_member_t  *foreign_member;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  if (!local_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnecting : local component without channel");
    return;
  }

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnecting : foreign component without channel");
    return;
  }

  /* verify data length */
  if (data_len != 5) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_disconnecting: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);
  data += sizeof(uint32_t);

  /* get reason */
  reason = unmarshalU8(data);
  data += sizeof(uint8_t);

  /* disconnecting should come from member so he is a member of our channel */
  foreign_member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
  if (foreign_member) {
    switch (protocol) {
      #if CONFIG_DMP
      case DMP_PROTOCOL_ID:
        if (local_component->callback)
          (*local_component->callback)(SDT_EVENT_DISCONNECT, local_component, foreign_component, false, NULL, 0, NULL);

        acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_disconnecting : channel %" PRIu16 ", protocol: %" PRIu32 ", reason %" PRIu8, local_component->tx_channel->number, protocol, reason);
        foreign_member->state = msJOINED;
        break;
      #endif
      default:
        acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_disconnecting: Unknown protocol");
    }
  } else {
    acnlog(LOG_WARNING | LOG_WARNING, "sdt_rx_disconnecting: No foreign member");
  }
  LOG_FEND();
}

/*****************************************************************************/
/*
 * resends contains the list of buffers in LIFO
 */
static int
sdt_save_buffer(rlp_txbuf_t *tx_buffer, uint8_t *pdup, uint32_t size, component_t *local_component)
{
  sdt_resend_t  *resend = NULL;
  sdt_resend_t  *last = NULL;

  LOG_FSTART();

  /* get new buffer*/
  resend = sdtm_new_resend();

  /* buffer full so flush tail */
  if (!resend) {
    /* start at top of stack */
    resend = resends;
    while (resend) {
      /* last one has no "next" */
      if (!resend->next) {
        acnlog(LOG_DEBUG | LOG_SDT,"sdt_save_buffer: buffer full truncating");

        /* expire it */
        resend->expires_ms = 0;
        /* free buffer */
        rlpm_release_txbuf(resend->tx_buffer);
        /* make a new tail */
        if (last) {
          last->next = NULL;
        }
        break;
      }
      last = resend;
      resend = resend->next;
    }
  }
  /* best have one by now! */
  assert(resend);

  /* increment use so it is not deleted */
  rlp_incuse(tx_buffer);
  /* copy needed data for resending */
  resend->reliable_seq = local_component->tx_channel->reliable_seq;
  resend->tx_buffer = tx_buffer;
  resend->pdup = pdup;
  resend->size = size;
  resend->local_component = local_component;

  /* now allow it to exprire in 1 second */
  resend->expires_ms = SDT_RESEND_TIMEOUT_ms;

  /* add it to top of stack */
  resend->next = resends;
  resends = resend;

  LOG_FEND();
  return OK;
}


/*****************************************************************************/
/*
 * Debug print stat on components
 *
 * TODO: This should be in "components.c"
 */
void sdt_stats(void)
{
  component_t *component;
  uint32_t i;
#if acntestlog(LOG_INFO | LOG_STAT)
  char  cid_text[CID_STR_SIZE];
#endif
  uint32_t  addr;
  uint16_t  port;
  sdt_channel_t *channel;
  sdt_member_t *member;

  for (component = allcomps, i = 0; component != NULL; component = component->next) {
    acnlog(LOG_INFO | LOG_STAT, "------------------------");
    acnlog(LOG_INFO | LOG_STAT, "Component: %" PRIu32, ++i);
#if acntestlog(LOG_INFO | LOG_STAT)
    acnlog(LOG_INFO | LOG_STAT, "CID: %s", cidToText(component->cid, cid_text));
    acnlog(LOG_INFO | LOG_STAT, "DCID: %s", cidToText(component->cid, cid_text));
#endif
    acnlog(LOG_INFO | LOG_STAT, "fctn: %s", component->fctn);
    acnlog(LOG_INFO | LOG_STAT, "uacn: %s", component->uacn);
    switch (component->access) {
      case accUNKNOWN: acnlog(LOG_INFO | LOG_STAT, "access: UNKNOWN"); break;
      case accCONTROL: acnlog(LOG_INFO | LOG_STAT, "access: CONTROL"); break;
      case accDEVICE: acnlog(LOG_INFO | LOG_STAT, "access: DEVICE"); break;
      case accBOTH: acnlog(LOG_INFO | LOG_STAT, "access: BOTH"); break;
    }
    acnlog(LOG_INFO | LOG_STAT, "local: %s", component->is_local?"true":"false");
    #if CONFIG_EPI10
      acnlog(LOG_INFO | LOG_STAT, "dyn_mcast: %d", component->dyn_mcast);
    #endif
    port = ntohs(netx_PORT(&component->adhoc_addr));
    addr = ntohl(netx_INADDR(&component->adhoc_addr));
    acnlog(LOG_INFO | LOG_STAT, "ad_hoc: %s: %d", ntoa(addr), port);
    acnlog(LOG_INFO | LOG_STAT, "ad_hoc_exp: %d", component->adhoc_expires_at);
    acnlog(LOG_INFO | LOG_STAT, "created_by: %d", component->created_by);
    acnlog(LOG_INFO | LOG_STAT, "dirty: %s", component->dirty?"true":"false");
    channel = component->tx_channel;
    if (channel) {
      acnlog(LOG_INFO | LOG_STAT, "chan-number: %d", channel->number);
      acnlog(LOG_INFO | LOG_STAT, " avail mid: %d", channel->available_mid);
      port = ntohs(netx_PORT(&channel->destination_addr));
      addr = ntohl(netx_INADDR(&channel->destination_addr));
      acnlog(LOG_INFO | LOG_STAT, " dest_addr: %s: %d", ntoa(addr), port);
      port = netx_PORT(&channel->source_addr);
      addr = netx_INADDR(&channel->source_addr);
      acnlog(LOG_INFO | LOG_STAT, " source_addr: %s: %d", ntoa(addr), port);
      acnlog(LOG_INFO | LOG_STAT, " total_seq: %" PRIu32, channel->total_seq);
      acnlog(LOG_INFO | LOG_STAT, " reliable_seq: %" PRIu32, channel->reliable_seq);
      acnlog(LOG_INFO | LOG_STAT, " oldest_avail: %" PRIu32, channel->oldest_avail);
      acnlog(LOG_INFO | LOG_STAT, " mak_ms: %d", channel->mak_ms);
      member = channel->member_list;
      while (member) {
        /* component_t    *component;    */ /*show cid? */
        acnlog(LOG_INFO | LOG_STAT, " mem-mid: %d", member->mid);
        acnlog(LOG_INFO | LOG_STAT, " mem-nak: %d", member->nak);
        switch (member->state) {
          case msDELETE          : acnlog(LOG_INFO | LOG_STAT, "mem-state: msDELETE"); break;
          case msEMPTY           : acnlog(LOG_INFO | LOG_STAT, "mem-state: msEMPTY"); break;
          case msWAIT_FOR_ACCEPT : acnlog(LOG_INFO | LOG_STAT, "mem-state: msWAIT_FOR_ACCEPT"); break;
          case msWAIT_FOR_ACK    : acnlog(LOG_INFO | LOG_STAT, "mem-state: msWAIT_FOR_ACK"); break;
          case msJOINED          : acnlog(LOG_INFO | LOG_STAT, "mem-state: msJOINED"); break;
          case msCONNECTED       : acnlog(LOG_INFO | LOG_STAT, "mem-state: msCONNECTED"); break;
        }
        acnlog(LOG_INFO | LOG_STAT, " mem-expiry_time_s: %d", member->expiry_time_s);
        acnlog(LOG_INFO | LOG_STAT, " mem-expires_ms: %d", member->expires_ms);
        acnlog(LOG_INFO | LOG_STAT, " mem-nak_holdoff: %d", member->nak_holdoff);
        acnlog(LOG_INFO | LOG_STAT, " mem-nak_modulus: %d", member->nak_modulus);
        acnlog(LOG_INFO | LOG_STAT, " mem-nak_max_wait: %d", member->nak_max_wait);
        member = member->next;
      }
    /* TODO: perhaps expand these someday! */
    /* struct netsocket_s    *sock; */
    /* struct rlp_listener_s *listener;     */ /* multicast listener */
    } else {
      acnlog(LOG_INFO | LOG_STAT, "channel: NULL");
    }
    if (component->callback) {
      acnlog(LOG_INFO | LOG_STAT, "callback: %" PRIxPTR, (uintptr_t)component->callback);
    }
  }
}

#endif /* CONFIG_SDT */
