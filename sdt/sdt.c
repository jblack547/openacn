/*--------------------------------------------------------------------*/
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

*/
/*--------------------------------------------------------------------*/
static const char *rcsid __attribute__ ((unused)) =
   "$Id$";

#include <string.h>
#include <stdio.h>

#include "opt.h"

#if CONFIG_SDT
#include "acn_arch.h"
#include "sdt.h"
#include "sdtmem.h"
#include "acn_sdt.h"
#include "rlp.h"
#include "acnlog.h"
#include "marshal.h"
#include "netiface.h"
#include "mcast_util.h"
#include "acn_port.h"

#if CONFIG_STACK_LWIP
#include "lwip/sys.h"
#define jiffies() sys_jiffies()
#endif

#if !CONFIG_RLP_SINGLE_CLIENT
/*
if your compiler does not conform to ANSI macro expansions it may
recurse infinitely on this definition.
*/
#define rlp_add_pdu(buf, pdudata, size, packetdatap) rlp_add_pdu(buf, pdudata, size, PROTO_SDT, packetdatap)
#endif

#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_SDT, "%s :...", __func__)
//#define LOG_FEND() acnlog(LOG_DEBUG | LOG_SDT, "%s :...", __func__)
//#define LOG_FSTART() 
#define LOG_FEND() 

#define CHANNEL_OUTBOUND_TRAFFIC 0
#define SDT_TMR_INTERVAL 1000

static component_t     *components = NULL;

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

//component_t *my_component;
//cid_t my_xcid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//cid_t my_xdcid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

struct netsocket_s    *sdt_adhoc_socket = NULL;
struct rlp_listener_s *sdt_adhoc_listener = NULL;
struct netsocket_s    *sdt_multicast_socket = NULL;
int sdt_started;
enum {
  ssCLOSED,
  ssSTARTED,
  ssCLOSING
};

uint16_t      sdt_next_member(sdt_channel_t *channel);
sdt_member_t *sdt_find_member_by_mid(sdt_channel_t *channel, uint16_t mid);
sdt_member_t *sdt_find_member_by_component(sdt_channel_t *channel, component_t *component);
component_t  *sdt_find_component(const cid_t cid);



//TODO: wrf these were moved header file for testing...
#if 0
/* BASE MESSAGES */
static void     sdt_tx_join(component_t *local_component, component_t *foreign_component);
static void     sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component);
static void     sdt_tx_join_refuse(const cid_t foreign_cid, component_t *local_component, const neti_addr_t *source_addr, 
                   uint16_t foreign_channel_num, uint16_t local_mid, uint32_t foreign_rel_seq, uint8_t reason);
static void     sdt_tx_leaving(component_t *foreign_component, component_t *local_component, sdt_member_t *local_member, uint8_t reason);
static void     sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed);
//TODO:         sdt_tx_get_sessions()
//TODO:         sdt_tx_sessions()

static void     sdt_rx_join(const cid_t foreign_cid, const neti_addr_t *source_addr, const uint8_t *join, uint32_t data_len);
static void     sdt_rx_join_accept(const cid_t foreign_cid, const uint8_t *join_accept, uint32_t data_len);
static void     sdt_rx_join_refuse(const cid_t foreign_cid, const uint8_t *join_refuse, uint32_t data_len);
static void     sdt_rx_leaving(const cid_t foreign_cid, const uint8_t *leaving, uint32_t data_len);
static void     sdt_rx_nak(const cid_t foreign_cid, const uint8_t *nak, uint32_t data_len);
static void     sdt_rx_wrapper(const cid_t foreign_cid, const neti_addr_t *source_addr, const uint8_t *wrapper,  bool is_reliable, uint32_t data_len);
//TODO:         sdt_rx_get_sessions()
//TODO:         sdt_rx_sessions()

/* WRAPPED MESSAGES */
static void     sdt_tx_ack(component_t *local_component, component_t *foreign_component);
//TODO:         sdt_tx_channel_params(sdt_wrapper_t *wrapper);
static void     sdt_tx_leave(component_t *local_component, component_t *foreign_component, sdt_member_t *foreign_member);
static void     sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
static void     sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_connect_refuse(sdt_wrapper_t *wrapper);
static void     sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_disconecting(sdt_wrapper_t *wrapper);
static void     sdt_tx_mak_all(component_t *local_component);


static void     sdt_rx_ack(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_channel_params();
static void     sdt_rx_leave(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_connect_refuse();
//TODO:         sdt_rx_disconnect();
//TODO:         sdt_rx_disconecting();

#endif

/* UTILITY FUNCTIONS */
static       uint8_t *marshal_channel_param_block(uint8_t *paramBlock);
static const uint8_t *unmarshal_channel_param_block(sdt_member_t *member, const uint8_t *param_block);
static       uint8_t *marshal_transport_address(uint8_t *data, const neti_addr_t *transport_addr, int type);
static const uint8_t *unmarshal_transport_address(const uint8_t *data, const neti_addr_t *transport_addr, neti_addr_t *packet_addr, uint8_t *type);
static       uint8_t *sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold);
static       void     sdt_client_rx_handler(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static       uint32_t check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail);
static       uint8_t *sdt_format_client_block(uint8_t *client_block, uint16_t foreignlMember, uint32_t protocol, uint16_t association);

#if 0
// wrf - not used at this time
enum
{
  SDT_STATE_NON_MEMBER,
  SDT_STATE_JOIN_SENT,
  SDT_STATE_JOIN_ACCEPT_SENT,
  SDT_STATE_JOIN_RECEIVED,
  SDT_STATE_CHANNEL_OPEN,
  SDT_STATE_LEAVING,  
};

static void
sdt_local_init(void)
{
  my_component = sdt_add_component(my_xcid, my_xdcid, true);
  if (!my_component) {
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_local_init : failed to get new component");
  return;
  }
}

/*****************************************************************************/
// wrf - not used at this time
struct localComponent_s {
  cid_t cid;
  acnProtocol_t protocols[MAXPROTOCOLS];
  int (*connectfilter)(cid_t, struct session_s sessionInfo, acnProtocol_t protocol);
};

/*****************************************************************************/
// wrf - not used at this time
int 
sdt_register(struct localComponent_s comp)
{
  sdt_add_component(comp, NULL, what?);
}
#endif

/* may go away, used for testing */
int sdt_get_adhoc_port(void)
{
  return NSK_PORT(*sdt_adhoc_socket);
}

/*****************************************************************************/
/* 
  Initialize SDT structures
    returns -1 if fails, 0 otherwise
 */
int
sdt_init(void)
{
  static bool initializedState = 0;

  /* Prevent reentry */
  if (initializedState) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_init: already initialized");
    return -1;
  }

  rlp_init();
  sdtm_init();
  initializedState = 1;
  return 0;
}

/*****************************************************************************/
/* 
  Kick off the SDT processes
    returns -1 if fails, 0 otherwise
 */
int
sdt_startup(bool acceptAdHoc)
{
  LOG_FSTART();


  /* prevent reentry */
  if (sdt_started != ssCLOSED) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_startup: already started");
    return -1;
  }

  if (acceptAdHoc) {
    /* Open ad hoc channel on ephemerql port */
    if (!sdt_adhoc_socket) {
      sdt_adhoc_socket = rlp_open_netsocket(0x5000);
//      sdt_adhoc_socket = rlp_open_netsocket(NETI_PORT_EPHEM);
      if (sdt_adhoc_socket) {
        sdt_adhoc_listener = rlp_add_listener(sdt_adhoc_socket, NETI_GROUP_UNICAST, PROTO_SDT, sdt_rx_handler, NULL);
        acnlog(LOG_DEBUG | LOG_SDT, "sdt_startup: adhoc port: %d", NSK_PORT(*sdt_adhoc_socket));
      }
    }
    if (!sdt_multicast_socket) {
      sdt_multicast_socket = rlp_open_netsocket(5568);
      /* Listeners for multicast will be added as needed */
    }
  }
  sdt_started = ssSTARTED;

  //TODO: add task to kick timer! (currently doing this at app layer)
  // addTask(5000, sdtTick, -1);  // PN-FIXME
  return 0;
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

  LOG_FSTART();

  /* Prevent reentry */
  if (sdt_started != ssSTARTED) {
    acnlog(LOG_WARNING | LOG_SDT,"sdt_shutdown: not started");
    return -1;
  }
  /* do this write away to prevent timer from hacking at these*/
  sdt_started = ssCLOSING;

  /* if we have a local component with members in the channel, tell them  to get out of town */
  component = components;
  while(component) {
    if (component->tx_channel) {
      member = component->tx_channel->member_list;
      if (component->tx_channel->is_local) {
        while(member) {
          sdt_tx_leave(component, member->component, member);
          member = member->next;
        }
      } else {
        while(member) {
          sdt_tx_leaving(component, member->component, member, SDT_REASON_NONSPEC);
          member = member->next;
        }
      }
    }
    component = component->next;
  }
  protect = ACN_PORT_PROTECT();
  /*  get first component and rip thru all of them (cleaning up resources along the way) */
  component = components;
  while(component) {
    /* returns the next component after delete */
    component = sdt_del_component(component);
  }
  ACN_PORT_UNPROTECT(protect);

  /* adhoc clean up */
  if (sdt_adhoc_listener && sdt_adhoc_socket) {
    rlp_del_listener(sdt_adhoc_socket, sdt_adhoc_listener);
  }
  sdt_adhoc_listener = NULL;

  if (sdt_adhoc_socket) {
    rlp_close_netsocket(sdt_adhoc_socket);
  }
  sdt_adhoc_socket = NULL;

  /* multicast clean up.  listeners for multicast are cleaned up with the associated channel */
  if (sdt_multicast_socket) {
    rlp_close_netsocket(sdt_multicast_socket);
  }
  sdt_multicast_socket = NULL;

  //TODO: stop timer task?
  sdt_started = ssCLOSED;
  return 0;
}

/*****************************************************************************/
/* 
  Add a new component and initialize it;
  returns new component or NULL
 */
component_t *
sdt_add_component(const cid_t cid, const cid_t dcid, bool is_local)
{
  component_t *component;

  #if LOG_DEBUG | LOG_SDT
  char  uuid_text[37];
  uuidToText(cid, uuid_text);
  #endif

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_component: %s", uuid_text);

  /* find and empty one */
  component = sdtm_new_component();
  if (component) {
    component->next = components;
    components = component;
    /* assign values */
    if (cid) 
      uuidCopy(component->cid, cid);
    if (dcid)
      uuidCopy(component->dcid, dcid);
    //component->adhoc_expires_at = // default 0
    //tx_channel = 0 // default
    if (is_local) {
      mcast_alloc_init(0, 0, component);
    }
    //is_receprocal     // default 0 (false)
    return component;
  }
  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_component: failed to add new component");

  return NULL; /* none left */
}


/*****************************************************************************/
/* 
  Delete a component and it's dependencies
  returns next component or NULL
 */
component_t *
sdt_del_component(component_t *component)
{
  component_t *cur;

  #if LOG_DEBUG | LOG_SDT
  char  uuid_text[37];
  uuidToText(component->cid, uuid_text);
  #endif

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_del_component: %s", uuid_text);

  /* remove our channel (and member list) */
  if (component->tx_channel) {
    sdt_del_channel(component);
  }

  /* if it is at top, then we just move leader */
  if (component == components) {
    components = component->next;
    sdtm_free_component(component);
    return components;
  }
  
  /* else run through list in order, finding previous one */
  cur = components;
  while (cur) {
    if (cur->next == component) {
      /* jump around it */
      cur->next = component->next;
      sdtm_free_component(component);
      return (cur->next);
    }
    cur = cur->next;
  }
  return NULL;
}

/*****************************************************************************/
/* 
  Add a new channel into component and initialize it;
  returns new channel or NULL
 */
sdt_channel_t *
sdt_add_channel(component_t *leader, uint16_t channel_number, bool is_local)
{
	sdt_channel_t *channel;

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_channel %d", channel_number);

  /* can't have channel number 0 */
  if (channel_number == 0) {
    return NULL; 
  }

  //TODO: should we check to see if this exists already?
  channel = sdtm_new_channel();
  if (channel) {
    /* assign this to our leader */
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
    leader->tx_channel = channel;
    return channel;
  }
  acnlog(LOG_ERR | LOG_SDT,"sdt_add_channel: failed to get new channel");
  return NULL; /* none left */
}

/*****************************************************************************/
/* 
  Delete a channel and it's dependencies from a component
  returns NULL (reserving return of next channel for later)
 */
sdt_channel_t *
sdt_del_channel(component_t *leader)
{
  sdt_member_t  *member;
  sdt_channel_t *channel;

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

  acnlog(LOG_DEBUG | LOG_SDT,"sdt_add_member: to channel %d", channel->number);

  // TODO: should we verify the component does not alread exist?
  /* find and empty one */
  member = sdtm_new_member();
  if (member) {
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
    return member;
  }
  acnlog(LOG_ERR | LOG_SDT,"sdt_new_member: none left");

  return NULL; /* none left */
}


/*****************************************************************************/
/* 
  Delete a member and it's dependencies from a channel
  returns next membe or NULL
 */
sdt_member_t *
sdt_del_member(sdt_channel_t *channel, sdt_member_t *member)
{
  sdt_member_t *cur;

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
uint16_t 
sdt_next_member(sdt_channel_t *channel)
{
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
  Find member by MID
*/
sdt_member_t *
sdt_find_member_by_mid(sdt_channel_t *channel, uint16_t mid)
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
sdt_find_member_by_component(sdt_channel_t *channel, component_t *component)
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
sdt_find_component(const cid_t cid)
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



/*****************************************************************************/
/*
  Callback from ACN RLP layer (listener)
    data        - pointer to SDT packet
    data_len    - length of SDT packet
    ref         - value set when setting up listener (not used)
    remhost     - network address of sending (foreign) host
    foreign_cid - cid of sending component (pulled from root layer)
 */
void
sdt_rx_handler(const uint8_t *data, int data_len, void *ref, const neti_addr_t *remhost, const cid_t foreign_cid)
{
  const uint8_t    *data_end;
  uint8_t     vector   = 0;
	uint32_t    data_size = 0;
	const uint8_t    *pdup, *datap;

  UNUSED_ARG(ref);

  LOG_FSTART();

  /* just in case something comes in while shutting down */
  if (sdt_started != ssSTARTED)
    return;
 
  /* local pointer */
	pdup = data;

  /* verify min data length */
  if (data_len < 3) {
		acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* On first packet, flags should all be set */
  // TODO: support for long packets?
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
        sdt_rx_wrapper(foreign_cid, remhost, datap, true, data_size);
        break;
      case SDT_UNREL_WRAPPER :
        acnlog(LOG_DEBUG | LOG_SDT,"sdtRxHandler: Dispatch to sdt_rx_wrapper as unreliable");
        sdt_rx_wrapper(foreign_cid, remhost, datap, false, data_size);
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
  } /* while() */

  LOG_FEND();
  return;
}

/*****************************************************************************/
/*
  Callback from OS to deal with session timeouts

  This function make the assumption that other threads that use the memory allocated 
  in sdtmem.c are of higher priority than this one and that a preemptive os is used. 
  By doing this, the higher priority threads will complete what they are doing before 
  giving up control.  This avoiding more complex locking schemes. It also assume that these
  tasks do not yield control back while in the middle of processing on the common memory.
*/
//TODO: this should be in opt.h as it is implementation dependent;
#define MS_PER_TICK 100
void 
sdt_tick(void *arg)
{
  component_t   *component;
  sdt_channel_t *channel;
  sdt_member_t  *member;
  int            do_mak;
  acn_protect_t  protect;

  UNUSED_ARG(arg);

  /* don't bother if we are not started */
  if (!sdt_started) 
    return;
  
  component = components;
  while(component) {
    channel = component->tx_channel;
    if (channel) {
      /* remove dead members */
      member = channel->member_list;
      while(member) {
        if (member->expires_ms > 0) {
          member->expires_ms -= MS_PER_TICK;
          if (member->expires_ms < 0) {
            member->expires_ms = 0;
          }
        }
        /* expired if zero */
        if (member->expires_ms == 0) {
          /* if channel is local, then all members are foreign */
          if (channel->is_local) {
            //command the member to leave my channel
            sdt_tx_leave(component, member->component, member);
            //dmp_comp_offline_notify(member->component);
          } else {
            /* send a leaving message */
            sdt_tx_leaving(component, member->component, member, SDT_REASON_CHANNEL_EXPIRED);
          }
        }
        /* this catches the case where expires_ms = -1 when created or when we have done responses and need to just delte*/
        if (member->expires_ms <= 0) {
          /* remove this member*/
          protect = ACN_PORT_PROTECT();
          member = sdt_del_member(channel, member);
          ACN_PORT_UNPROTECT(protect);
        } else {
          member = member->next;
        }
      }
      /* if the leader is local, send MAK if mak time out     */
      /* If any device needs MAK, then we will send it to all */
      if (channel->is_local) {
        do_mak = 0;
        member = channel->member_list;
        while(member) {
          if (member->state == msJOINED) {
            if (member->mak_ms > 0) {
              member->mak_ms -= MS_PER_TICK;
              if ((int)member->mak_ms < 0 ) {
                member->mak_ms = 0;
              }
            }
            if (member->mak_ms == 0) {
              /* reset timer and set flag */
              member->mak_ms = 1000;      // TODO: put this as an option someware
              do_mak = 1;
            }
          }
          member = member->next;
        }
        if (do_mak) {
          sdt_tx_mak_all(component);
        }
      }
      /* if we have no member, delete channel */
      if (!channel->member_list) {
        protect = ACN_PORT_PROTECT();
        sdt_del_channel(component);
        ACN_PORT_UNPROTECT(protect);
      }
    } /* of channels */

    if (!channel && component->auto_created) {
      component = sdt_del_component(component);
    } else {
      component = component->next;
    }
  } /* of while */
  /* Reset timers */
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
  param_block = marshalU8(param_block,  10);//FOREIGN_MEMBER_EXPIRY_TIME_ms/1000);
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
marshal_transport_address(uint8_t *data, const neti_addr_t *transport_addr, int type)
{
  switch(type) {
    case SDT_ADDR_NULL :
      data = marshalU8(data, SDT_ADDR_NULL);
      break;
    case SDT_ADDR_IPV4 :
      data = marshalU8(data, SDT_ADDR_IPV4);
      data = marshalU16(data, NETI_PORT(transport_addr));
      data = marshalU32(data, htonl(NETI_INADDR(transport_addr)));
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
unmarshal_transport_address(const uint8_t *data, const neti_addr_t *transport_addr, neti_addr_t *packet_addr, uint8_t *type)
{
  /* get and save type */
*type = unmarshalU8(data);
  data += sizeof(uint8_t);
  
  switch(*type) {
    case SDT_ADDR_NULL :
      NETI_PORT(packet_addr) = NETI_PORT(transport_addr);
      NETI_INADDR(packet_addr) = NETI_INADDR(transport_addr);
      break;
    case SDT_ADDR_IPV4 :
      NETI_PORT(packet_addr) = unmarshalU16(data);
      data += sizeof(uint16_t);
      NETI_INADDR(packet_addr) = htonl(unmarshalU32(data));
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
static uint8_t *
sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold)
{
  LOG_FSTART();

  /* Skip flags/length fields for now*/
  wrapper += sizeof(uint16_t);

  /* incremenet our sequence numbers... */
  if (is_reliable) {
    local_channel->reliable_seq++;
    local_channel->oldest_avail = (local_channel->reliable_seq - local_channel->oldest_avail == SDT_NUM_PACKET_BUFFERS) ? 
                                   local_channel->oldest_avail + 1 : local_channel->oldest_avail;
  }
  local_channel->total_seq++;

  /* wrawpper type */
  wrapper = marshalU8(wrapper, (is_reliable) ? SDT_REL_WRAPPER : SDT_UNREL_WRAPPER);
  /* channel number */
  wrapper = marshalU16(wrapper, local_channel->number);
  wrapper = marshalU32(wrapper, local_channel->total_seq);
  wrapper = marshalU32(wrapper, local_channel->reliable_seq);

  //FIXME - Change to oldest available when buffering support added
  wrapper = marshalU32(wrapper, local_channel->oldest_avail); 
  wrapper = marshalU16(wrapper, first_mid);
  wrapper = marshalU16(wrapper, last_mid);
  wrapper = marshalU16(wrapper, mak_threshold);

  /* return pointer to client block */
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
	const uint8_t    *pdup, *datap;
  uint8_t           vector   = 0;
	uint32_t          data_size = 0;
            
  LOG_FSTART();

  /* verify min data length */
  if (data_len < 3) {
		acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_handler: Packet too short to be valid");
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
        sdt_rx_ack(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CHANNEL_PARAMS :
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: SDT_CHANNEL_PARAMS not supported..");
        break;
      case SDT_LEAVE :
        sdt_rx_leave(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT :
        sdt_rx_connect(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT_ACCEPT :
        sdt_rx_connect_accept(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT_REFUSE :
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: Our Connect was Refused ??? ");
        break;
      case SDT_DISCONNECT :
        acnlog(LOG_INFO | LOG_SDT,"sdt_client_rx_handler: rx sdt DISCONNECT (NOP)");
        break;
      case SDT_DISCONNECTING :
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
    oldest_avail  - packets oldest avail valie
    returns       - SEQ_VALID if packet is correctly sequenced or error code
*/
static uint32_t 
check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail)
{
  int diff;
  
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
        if (diff) {
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
static 
uint8_t *
sdt_format_client_block(uint8_t *client_block, uint16_t foreign_mid, uint32_t protocol, uint16_t association)
{
  LOG_FSTART();

  /* skip flags and length for now */
  client_block += 2; 
  
  client_block = marshalU16(client_block, foreign_mid); 	/* Vector */
  client_block = marshalU32(client_block, protocol);		  /* Header, Client protocol */
  client_block = marshalU16(client_block, association); 	/* Header, Association */
  
  /* returns pointer to opaque datagram */
  return client_block; 
}

/*****************************************************************************/
/* 
  Convert seconds to system units
 */
static inline
uint32_t 
sec_to_ticks(int time_in_sec)
{
  return time_in_sec * 1000;
}

static inline
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
void 
sdt_tx_join(component_t *local_component, component_t *foreign_component)
{
  sdt_member_t  *foreign_member;
  sdt_channel_t *local_channel;
  sdt_channel_t *foreign_channel;
  uint8_t *buf_start;
  uint8_t *buffer;
  int      allocations = 0;

  rlp_txbuf_t *tx_buffer;
  
  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);

  auto void remove_allocations(void);
  void remove_allocations(void) {
//    if (allocations & ALLOCATED_LOCAL_MEMBER) sdt_del_member(foreign_channel, local_member);
//    if (allocations & ALLOCATED_FOREIGN_LISTENER) rlp_del_listener(sdt_multicast_socket, foreign_channel->listener);
//    if (allocations & ALLOCATED_FOREIGN_CHANNEL) sdt_del_channel(foreign_component);
//    if (allocations & ALLOCATED_FOREIGN_COMP) sdt_del_component(foreign_component);
    if (allocations & ALLOCATED_FOREIGN_MEMBER) sdt_del_member(local_channel, foreign_member);
    if (allocations & ALLOCATED_LOCAL_CHANNEL) sdt_del_channel(local_component);
  }

  local_channel = local_component->tx_channel;
  foreign_channel = foreign_component->tx_channel;

  /* if we don't have a channel, create one */
  if (!local_channel) {
    // initialize multicast address
    if (!local_component->dyn_mcast) {
      if (!mcast_alloc_init(0,0,local_component)) {
        acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : unable to allocate multicast address");
        return;
      }
    }
    // create channel
    local_channel = sdt_add_channel(local_component, rand(), true);
    if (!local_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : unable to add channel");
      return;
    }
    allocations |= ALLOCATED_LOCAL_CHANNEL;
    // assign channel address
    NETI_PORT(&local_channel->destination_addr) = 5568;
    NETI_INADDR(&local_channel->destination_addr) = mcast_alloc_new(local_component);
  }

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_join : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* Sanity check  */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (foreign_member) {
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join : already a member -- suppressing join");
    remove_allocations();
    return;
  }  

  /* put foreign component in our local channel list */
  foreign_member = sdt_add_member(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_join : failed to allocate foreign member");
    remove_allocations();
    return;
  }
  allocations |= ALLOCATED_FOREIGN_MEMBER;

  /* set mid */
  foreign_member->mid = sdt_next_member(local_channel);
  /* Timeout if JOIN ACCEPT and followup ACK is not received */
  foreign_member->expires_ms = RECIPROCAL_TIMEOUT_ms;
  /* set move flag up one state */
  foreign_member->state = msPENDING;

  /* Create packet buffer*/
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : failed to get new txbuf");
    remove_allocations();
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  // TODO: add unicast/multicast support?
  buffer = marshalU16(buffer, 49 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  //buffer = marshalU16(buffer, 43 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer = marshalU8(buffer, SDT_JOIN);
  buffer = marshalUUID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_member->mid);
  buffer = marshalU16(buffer, local_channel->number);
  if (foreign_channel)
    buffer = marshalU16(buffer, foreign_channel->number); /* Reciprocal Channel */
  else
    buffer = marshalU16(buffer, 0);                                     /* Reciprocal Channel */
  buffer = marshalU32(buffer, local_channel->total_seq);
  buffer = marshalU32(buffer, local_channel->reliable_seq);
  
  // TODO: add unicast/multicast support. 0 is UNICAST
  //buffer = marshalU8(buffer, 0);
  buffer = marshal_transport_address(buffer, &local_channel->destination_addr, SDT_ADDR_IPV4);
  buffer = marshal_channel_param_block(buffer);
  buffer = marshalU8(buffer, 255); //TODO, value?

  /* JOINS are always sent to foreign compoents adhoc address */
  /* and from a adhoc address                                 */
  rlp_add_pdu(tx_buffer, buf_start, 49, NULL);
  //rlp_add_pdu(tx_buffer, buf_start, 43, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join : channel %d", local_channel->number);

  /* if we have a tx_channel */
  if (foreign_channel) {
    rlp_send_block(tx_buffer, local_channel->sock, &foreign_channel->source_addr);
  } else {
    rlp_send_block(tx_buffer, local_channel->sock, &foreign_component->adhoc_addr);
  }
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
/*
  Send JOIN_ACCEPT
    local_member      - local member
    local_component   - local component
    foreign_component - foreign component

    foreign_component must have a tx_channel 
*/
/*static*/ void 
sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *tx_buffer;

  LOG_FSTART();

  assert(local_member);
  assert(local_component);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join_accept : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer = marshalU16(buffer, 29 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer = marshalU8(buffer, SDT_JOIN_ACCEPT);
  /* data */
  buffer = marshalUUID(buffer, foreign_component->cid);
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
  rlpm_freetxbuf(tx_buffer);
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
/*static*/ void 
sdt_tx_join_refuse(const cid_t foreign_cid, component_t *local_component, const neti_addr_t *source_addr, 
                   uint16_t foreign_channel_num, uint16_t local_mid, uint32_t foreign_rel_seq, uint8_t reason)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *tx_buffer;

  LOG_FSTART();

  assert(local_component);

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join_refuse : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer = marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer = marshalU8(buffer, SDT_JOIN_REFUSE);
  /* data */
  buffer = marshalUUID(buffer, foreign_cid);
  buffer = marshalU16(buffer, foreign_channel_num); /* leader's channel # */
  buffer = marshalU16(buffer, local_mid);           /* mid that the leader was assigning to me */
  buffer = marshalU32(buffer, foreign_rel_seq);     /* leader's channel rel seq # */
  buffer = marshalU8(buffer, reason);
  
  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 28, NULL);

  /* send via our ad-hoc to address it came from */
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_join_refuse : channel %d", foreign_channel_num);
  rlp_send_block(tx_buffer, sdt_adhoc_socket, source_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
/*
  Send LEAVING
    foreign_component   - component receiving
    local_component     - component sending 
    local_member        - local member in foreign channel
    reason              - reason we are leaving
*/
/*static*/ void 
sdt_tx_leaving(component_t *foreign_component, component_t *local_component, sdt_member_t *local_member, uint8_t reason)
{
  uint8_t       *buf_start;
  uint8_t       *buffer;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;

  LOG_FSTART();

  assert(local_component);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  foreign_channel = foreign_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_leaving : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  buffer = marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer = marshalU8(buffer, SDT_LEAVING);
  buffer = marshalUUID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_channel->number);
  buffer = marshalU16(buffer, local_member->mid);
  buffer = marshalU32(buffer, foreign_channel->reliable_seq);
  buffer = marshalU8(buffer, reason);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 28, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_leaving : channel %d", foreign_channel->number);

  /* send via channel */
  rlp_send_block(tx_buffer, sdt_adhoc_socket, &foreign_channel->source_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
/*
  Send NAK
    foreign_component   - component receiving
    local_component     - component sending 
    last_missed         - last missed sequence number (we assume the first from channel)
*/
/*static*/ void 
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
  assert(foreign_component->tx_channel);

  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_nak : failed to find local_member");
    return;
  }

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_nak : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  //FIXME -  add proper nak storm suppression?
  buffer = marshalU16(buffer, 35 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer = marshalU8(buffer, SDT_NAK);
  buffer = marshalUUID(buffer, foreign_component->cid);
  buffer = marshalU16(buffer, foreign_channel->number);
  buffer = marshalU16(buffer, local_member->mid);
  buffer = marshalU32(buffer, foreign_channel->reliable_seq);		/* last known good one */
  buffer = marshalU32(buffer, foreign_channel->reliable_seq + 1);	/* then the next must be the first I missed */
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
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
//TODO:         sdt_tx_get_sessions()

/*****************************************************************************/
//TODO:         sdt_tx_sessions()

/*****************************************************************************/
/*
  Received JOIN message
    foreign_cid    - cid of component that sent the message
    source_addr    - address JOIN message was received from
    join           - points to DATA section of JOIN message
    data_len       - length of DATA section.
*/

/*static*/ void
sdt_rx_join(const cid_t foreign_cid, const neti_addr_t *source_addr, const uint8_t *join, uint32_t data_len)
{
  cid_t           local_cid;
  const uint8_t  *joinp;
  uint16_t        foreign_channel_number;
  uint16_t        local_channel_number;
  
  uint16_t         local_mid;
  component_t     *local_component;
  component_t     *foreign_component;

  sdt_channel_t   *foreign_channel;
  sdt_channel_t   *local_channel;
  sdt_member_t    *local_member;
  uint32_t         foreign_total_seq;
  uint32_t         foreign_reliable_seq;
  
  uint8_t          address_type;
  uint8_t          adhoc_expiry;
  groupaddr_t      groupaddr;
  int              allocations = 0;


  LOG_FSTART();

  auto void remove_allocations(void);
  void remove_allocations(void) {
    if (allocations & ALLOCATED_LOCAL_MEMBER) sdt_del_member(foreign_channel, local_member);
    if (allocations & ALLOCATED_FOREIGN_LISTENER) rlp_del_listener(sdt_multicast_socket, foreign_channel->listener);
    if (allocations & ALLOCATED_FOREIGN_CHANNEL) sdt_del_channel(foreign_component);
    if (allocations & ALLOCATED_FOREIGN_COMP) sdt_del_component(foreign_component);
    if (allocations & ALLOCATED_LOCAL_CHANNEL) sdt_del_channel(local_component);
  }

  /* verify data length */  
  if (data_len < 40) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: pdu too short");
    return;
  }

  /* get local pointer */
  joinp = join;

  /* destination cid */
  unmarshalUUID(joinp, local_cid);
  joinp += UUIDSIZE;

  /* see if this is for one of our components */
  local_component = sdt_find_component(local_cid);
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

  local_channel = local_component->tx_channel;

  /* if we don't have a channel, create one */
  if (!local_channel) {
    // initialize multicast address
    if (!local_component->dyn_mcast) {
      if (!mcast_alloc_init(0,0,local_component)) {
        acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join : unable to allocate multicast address");
        sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
        remove_allocations();
        return;
      }
    }
    // create channel
    local_channel = sdt_add_channel(local_component, rand(), true);
    if (!local_channel) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join : failed to add local channel");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_LOCAL_CHANNEL;
    // assign channel address
    NETI_PORT(&local_channel->destination_addr) = 5568;
    NETI_INADDR(&local_channel->destination_addr) = mcast_alloc_new(local_component);
  }

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_join : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* if we have reciprocal, then verify it is correct */
  if (local_channel_number) {
    if (local_component->tx_channel->number != local_channel_number) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: reciprocal channel number does not match");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_NO_RECIPROCAL);
      remove_allocations();
      return;
    }
  }

  // TODO: possible callback to app to verify accept
  
  /*  Are we already tracking the src component, if not add it */
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component) {
    foreign_component = sdt_add_component(foreign_cid, NULL, false);
    if (!foreign_component) { /* allocation failure */
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: failed to add foreign component");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }  
    foreign_component->auto_created = true;
    allocations |= ALLOCATED_FOREIGN_COMP;
  }
  
  /* foreign channel normally would not exist yet! */
  foreign_channel = foreign_component->tx_channel;
  if (foreign_channel) {
    /* we only can have one channel so it better be the same */
    if (foreign_channel->number != foreign_channel_number) {
      acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join: multiple channel rejected", foreign_channel_number);
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join: pre-existing foreign channel %d", foreign_channel_number);
  } else {
    /* add foreign channel */
    foreign_channel = sdt_add_channel(foreign_component, foreign_channel_number, false);
    if (!foreign_channel) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: failed to add foreign channel");
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_FOREIGN_CHANNEL;
  }
  foreign_channel->total_seq = foreign_total_seq;
  foreign_channel->reliable_seq = foreign_reliable_seq;
  foreign_channel->source_addr = *source_addr;

  joinp = unmarshal_transport_address(joinp, source_addr, &foreign_channel->destination_addr, &address_type);

  if (address_type == SDT_ADDR_IPV6) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: Unsupported downstream Address Type");
    sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_BAD_ADDR);
    remove_allocations();
    return;
  }

  if (address_type == SDT_ADDR_IPV4) {
    groupaddr = NETI_INADDR(&foreign_channel->destination_addr);
    if (is_multicast(groupaddr)) {
      foreign_channel->listener = rlp_add_listener(sdt_multicast_socket, groupaddr, PROTO_SDT, sdt_rx_handler, NULL);
      if (!foreign_channel->listener) {
        acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: failed to add multicast listener");
        sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
        remove_allocations();
        return;
      }
      allocations |= ALLOCATED_FOREIGN_LISTENER;
    } else {
      // TODo: gee what do I do here?
      
    }
  }

  local_member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    /* add local component to foreign channel */
    local_member = sdt_add_member(foreign_channel, local_component);
    if (!local_member) {
      acnlog(LOG_ERR | LOG_SDT, "sdt_rx_join: failed to add local member");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_RESOURCES);
      remove_allocations();
      return;
    }
    allocations |= ALLOCATED_LOCAL_MEMBER;

    /* set mid */
    local_member->mid = local_mid;
    /* set move flag up one state */
    local_member->state = msPENDING;
  } else {
    /* verify mid */
    if (local_member->mid != local_mid) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join: have local member but MID does not match");
      sdt_tx_join_refuse(foreign_cid, local_component, source_addr, foreign_channel_number, local_mid, foreign_reliable_seq, SDT_REASON_ALREADY_MEMBER);
      remove_allocations();
      return;
    }
  }

  /* fill in the member structure */
  joinp = unmarshal_channel_param_block(local_member, joinp);
  adhoc_expiry = unmarshalU8(joinp);
  joinp += sizeof(uint8_t);

  /* set time out */
  local_member->expires_ms = local_member->expiry_time_s * 1000;

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join : channel %d, mid %d", foreign_channel->number, local_mid);

  /* send the join */
  sdt_tx_join_accept(local_member, local_component, foreign_component);

  if (!local_channel_number) {
    sdt_tx_join(local_component, foreign_component);
  }
  return;
}

/*****************************************************************************/
/*
  Receive JOIN_ACCEPT
    foreign_cid    - source of component that sent the JOIN_ACCEPT
    join_accept    - points to DATA section of JOIN_ACCEPT message
    data_len       - length of DATA section.
*/
/*static*/ void
sdt_rx_join_accept(const cid_t foreign_cid, const uint8_t *join_accept, uint32_t data_len)
{
  component_t     *local_component;
  component_t     *foreign_component;
  sdt_member_t    *local_member;
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
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: foreign_component not found");
    return;
  }

  /* verify leader CID */
  unmarshalUUID(join_accept, local_cid);
  join_accept += UUIDSIZE;
  local_component = sdt_find_component(local_cid);
  if (!local_component) {
    acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_join_accept: not for me");
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
  foreign_member = sdt_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: MID not found in channel");
    return;
  }

  //TODO: do something with this??
  rel_seq_number = unmarshalU32(join_accept);
  join_accept += sizeof(uint32_t);

  /* if foreign component has a channel, it should match */
  foreign_channel_number = unmarshalU16(join_accept);
  join_accept += sizeof(uint16_t);
  if (foreign_component->tx_channel) {
    if (foreign_component->tx_channel->number != foreign_channel_number) {
      acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: foreign channel number mismatch");
      return;
    } else {
      /* we should send an ACK now */
      local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
      if (local_member) {
        if (local_member->state == msPENDING) {
          sdt_tx_ack(local_member->component, foreign_component);
        }
      } else {
        acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_accept: local member not found");
      }
    }
  } 

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join_accept : channel %d, mid %d", local_channel_number, foreign_mid);

  return;
}

/*****************************************************************************/
/*
  Receive JOIN_REFUSE
    foreign_cid    - source of component that sent the JOIN_REFUSE
    join_refuse    - points to DATA section of JOIN_REFUSE message
    data_len       - length of DATA section.
*/
/*static*/ void
sdt_rx_join_refuse(const cid_t foreign_cid, const uint8_t *join_refuse, uint32_t data_len)
{
  cid_t             local_cid;
  component_t      *local_component;
  uint16_t          local_channel_number;
  component_t      *foreign_component;
  uint16_t          foreign_mid;
  uint32_t          rel_seq_num;
  uint8_t           reason_code;

  LOG_FSTART();

  /* verify data length */
  if (data_len != 25) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_join_refuse: foreign_component not found");
    return;
  }

  /* get leader */
  unmarshalUUID(join_refuse, local_cid);
  join_refuse += UUIDSIZE;
  local_component = sdt_find_component(local_cid);
  if (!local_component) {
    acnlog(LOG_NOTICE | LOG_SDT, "sdt_rx_join_refuse: Not for me");
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

  //TODO: add reason to text
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_join_refuse: channel %d, mid, %d, reason %d", local_channel_number, foreign_mid, reason_code);

  //TODO: inform application>
  //      cleanup open channels and such?
  return;
}

/*****************************************************************************/
/*
  Receive LEAVING
    foreign_cid    - source of component that send the LEAVING message
    leaving        - points to DATA section of LEAVING message
    data_len       - length of DATA section.
*/
/*static*/ void
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
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: foreign_component not found");
    return;
  }

  /* get leader cid */
  unmarshalUUID(leaving, local_cid);
  leaving += UUIDSIZE;
  local_component = sdt_find_component(local_cid);
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
  foreign_member = sdt_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (!foreign_member)  {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_leaving: member not found");
    return;
  }

  // TODO: implement DMP or callback
  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_leaving : channel %d, mid %d", local_channel_number, foreign_mid);

  /* ok now remove it on next tick*/
  foreign_member->expires_ms = -1;
  return;
}

/*****************************************************************************/
/*
  Receive NAK
    foreign_cid    - source of component sending NAK
    nak            - points to DATA section of NAK message
    data_len       - length of DATA section.
*/
/*static*/ void 
sdt_rx_nak(const cid_t foreign_cid, const uint8_t *nak, uint32_t data_len)
{
  cid_t            local_cid;
  component_t      *local_component;
  uint16_t         local_channel_number;
  component_t     *foreign_component;
   
  uint32_t        first_missed;
  uint32_t        last_missed;
  int             num_back;
  
  LOG_FSTART();

  /* verify data length */
  if (data_len != 32) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: foreign_component not found");
    return;
  }
 
  /* get Leader CID */
  unmarshalUUID(nak, local_cid);
  nak += UUIDSIZE;
  local_component = sdt_find_component(local_cid);
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

  num_back = local_component->tx_channel->reliable_seq - first_missed;
  if (first_missed < local_component->tx_channel->oldest_avail)  {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_nak: Requested Wrappers not avail");
    return;
  }

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_nak : channel %d", local_channel_number);
  
  last_missed = unmarshalU32(nak);
  nak += sizeof(uint32_t);
  while(first_missed != last_missed) {
    // TODO: wrf implement resend?
    //rlpResendTo(channel->sock, channel->downstream, channel->downstreamPort, numBack);
    num_back--;
    first_missed++;
  }
}

/*static*/ void
sdt_resend(sdt_channel_t channel, uint16_t reliableSeq)
{
  UNUSED_ARG(channel);
  UNUSED_ARG(reliableSeq);
  //TODO:
  /*
    1) Find buffer and tag it as needing to be resetn
    2) After delay, we should send it (see epi18.h for times)
    3) When its time to send, copy data to new buffer, update the oldestAvail field and resend without putting 
       this packet into the queue list..
   */
}


/*****************************************************************************/
/*
  Receive a REL_WRAPPER or UNREL_WRAPPER packet
    foreign_cid    - cid of component that sent the wrapper
    source_addr    - address wrapper came from
    is_reliable    - flag indicating if type of wrapper
    wrapper        - pointer to wrapper DATA block
    data_len       - length of data block
*/
/*static*/ void
sdt_rx_wrapper(const cid_t foreign_cid, const neti_addr_t *source_addr, const uint8_t *wrapper,  bool is_reliable, uint32_t data_len)
{
  component_t     *foreign_component;
  sdt_channel_t   *foreign_channel;
  sdt_member_t    *local_member;
  sdt_member_t    *foreign_member;
  
  const uint8_t    *wrapperp;
  const uint8_t    *data_end;
	uint32_t          data_size = 0;
	const uint8_t    *pdup, *datap;

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

  UNUSED_ARG(source_addr);

  LOG_FSTART();

  /* verify length */
  if (data_len < 20) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: Invalid data_len");
    return;
  }
  data_end = wrapper + data_len;

  /* verify we are tracking this component */
  foreign_component = sdt_find_component(foreign_cid);
  if (!foreign_component)  {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: Not tracking this component");
    return;
  }

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
        sdt_tx_nak(foreign_component, local_member->component, reliable_seq);
        local_member = local_member->next;
      }
      return;
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
        sdt_tx_leaving(foreign_component, local_member->component, local_member, SDT_REASON_LOST_SEQUENCE);
        /* remove on next tick */
        local_member->expires_ms = -1;
        local_member = local_member->next;
      }
      return;
      break;
  }

  //TODO: Currently doesn't care about the MAK Threshold

  /* send acks if needed */
  local_member = foreign_channel->member_list;
  while (local_member) {
    if ((local_member->state == msJOINED) && ((local_member->mid >= first_mid) && (local_member->mid <= last_mid)))  {
      /* send ack */
      sdt_tx_ack(local_member->component, foreign_component);
      /* a MAK is "data on the channel" so we can reset the expiry timeout */
      /* this can get set again based on the wrapped packet MID */
      local_member->expires_ms = local_member->expiry_time_s * 1000;
    }
    local_member = local_member->next;
  }

  /* start of our client block containing one or multiple pdus*/
  pdup = wrapperp;
  
  /* check to see if we have any packets */
  if (pdup == data_end) {
		acnlog(LOG_DEBUG | LOG_SDT,"sdt_rx_wrapper: wrapper with no packets");
    return;
  }

  /* check first packet */
	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		acnlog(LOG_WARNING | LOG_SDT,"sdt_rx_wrapper: illegal first PDU flags");
		return;
	}

  /* decode and dispatch the wrapped pdus (Wrapped SDT or DMP) */
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
    local_member = foreign_channel->member_list;
    while (local_member) {
      if ((local_mid == 0xFFF) || (local_member->mid == local_mid)) {
        if (association) {
          /* the association channel is the local channel */
          if (local_member->component->tx_channel->number != association) {
            acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: association channel not found");
            return;
          } else {
            /* now find the foreign component in the local channel and mark it as having been acked */
            foreign_member = sdt_find_member_by_component(local_member->component->tx_channel, foreign_component);
            if (foreign_member) {
              foreign_member->mak_ms = 1000; //TODO: un-hard code this
            } else {
              acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_wrapper: association channel member not found");
            }
          }
        }
        /* we have data on this channel so reset the expiry timeout */
        local_member->expires_ms = local_member->expiry_time_s * 1000;

        if (protocol == PROTO_SDT) {
          sdt_client_rx_handler(local_member->component, foreign_component, datap, data_size);
        } //else {
//        rx_handler = sdt_get_rx_handler(clientProtocol,ref)    
//        if(!rx_handler) {
//          acnlog(LOG_DEBUG | LOG_SDT,"sdt_rx_wrapper: Unknown Vector - skip");
//        } else {
//          rx_handler(remoteLeader->component,member->component,
//          remoteChannel, clientPdu.data, clientPdu.dataLength, ref);
//        }        
//      }
      }
      local_member = local_member->next;
    }
  }

  #define LOG_FEND() 
  return;
}

/*****************************************************************************/
//TODO:         sdt_rx_get_sessions()

/*****************************************************************************/
//TODO:         sdt_rx_sessions()

/*****************************************************************************/
/*
  Send ACK
    local_component   -  
    foreign_component -

  ACK is sent in a reliable wrapper
*/
/*static*/ void
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
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get foreign member in local channel */
  /* this will be used to get the MID to send it to */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_ack : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_ack : failed to get new txbuf");
    return;
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
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (ACK message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_ACK);
  datagram = marshalU32(datagram, foreign_channel->reliable_seq);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_ack : channel %d", foreign_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
//TODO:         sdt_tx_channel_params();

/*****************************************************************************/
/*
  Send LEAVE
    local_component   - local component sending LEAVE
    foreign_component - foreign component receiving
    foreign_member    - foreign member in local channel

  LEAVE is sent in a reliable wrapper
*/
/*static*/ void 
sdt_tx_leave(component_t *local_component, component_t *foreign_component, sdt_member_t *foreign_member)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_member);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_leave : failed to get new txbuf");
    return;
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
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram (LEAVE message) */
  datagram = marshalU16(datagram, 3 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_LEAVE);

  /* add length to client block pdu */
  marshalU16(client_block, (10+3) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);

  acnlog(LOG_DEBUG | LOG_SDT, "sdt_tx_leave : channel %d", local_channel->number);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
/*
  Send CONNECT
    local_component   - local component sending CONNECT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  CONNECT is sent in a reliable wrapper
*/
/*static*/ void 
sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol)
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
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_connect : failed to get new txbuf");
    return;
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
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram CONNECT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_CONNECT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
/*
  Send CONNECT_ACCEPT
    local_component   - local component sending CONNECT_ACCEPT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  CONNECT_ACCEPT is sent in a reliable wrapper
*/
/*static*/ void 
sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol)
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
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_connect_accept : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_connect_accept : failed to get new txbuf");
    return;
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
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (CONNECT_ACCEPT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_CONNECT_ACCEPT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
//TODO:         sdt_tx_connect_refuse();

/*****************************************************************************/
/*
  Send DISCONNECT
    local_component   - local component sending DISCONNECT
    foreign_component - foreign component receiving
    protocol          - DMP we hope!

  DISCONNECT is sent in a reliable wrapper
*/
/*static*/ void 
sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol)
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
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* get local member in foreign channel */
  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_tx_disconnect : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_disconnect : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* we must have a local socket */
  if (!local_channel->sock) {
    acnlog(LOG_INFO | LOG_SDT, "sdt_tx_disconnect : assuming ad-hoc socket");
    local_channel->sock = sdt_adhoc_socket;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, CHANNEL_OUTBOUND_TRAFFIC);

  /* add datagram (DISCONNECT message) */
  datagram = marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, SDT_DISCONNECT);
  datagram = marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}

/*****************************************************************************/
//TODO:         sdt_tx_disconecting();

/*****************************************************************************/
/*
  Send MAK
    local_component   - local component sending MAK

  This is a psudo command as it just sends an empty wrapper with MAK flags set
*/
/*static*/ void
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

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_tx_mak_all : failed to get new txbuf");
    return;
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
  rlpm_freetxbuf(tx_buffer);
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
/*static*/ void 
sdt_rx_ack(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t       reliableSeq;
  sdt_member_t  *member;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

  LOG_FSTART();

  assert(local_component);
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* just to make it easier */
  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;


  /* verify data length */
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack: Invalid data_len");
    return;
  }

  member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : failed to get foreign_member");
    return;
  }

  //TODO: how should this be processed
  reliableSeq = unmarshalU32(data);
  
  /* reset timer */
  member->expires_ms = FOREIGN_MEMBER_EXPIRY_TIME_ms;
  
  /* we should send an ACK now */
  if (member->state == msPENDING) {
    sdt_tx_ack(local_component, member->component);
    member->state = msJOINED;
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_ack : foreign member %d JOINED", member->mid);
  }

  /* get local member */
  if (!foreign_channel) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : foreign component without channel");
    return;
  }
  member = sdt_find_member_by_component(foreign_channel, local_component);
  if (!member) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_ack : failed to get local_member");
    return;
  }
  if (member->state == msPENDING) {
    member->state = msJOINED;
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_ack : local member %d JOINED", member->mid);
  }
 

  return;
}

/*****************************************************************************/
//TODO:         sdt_rx_channel_params();

/*****************************************************************************/
/*
  Receive LEAVE
    local_component   - local component receiving LEAVE
    foreign_component - foreign component sending
    data              - pointer to data section of LEAVE
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_leave(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  sdt_member_t *local_member;

  UNUSED_ARG(data);

  LOG_FSTART();

  assert(local_component);
  assert(local_component->tx_channel);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  /* verify data length */  
  if (data_len != 0) {
    acnlog(LOG_ERR | LOG_SDT, "sdt_rx_leave: Invalid data_len");
    return;
  }

  local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
  if (local_member) {
    /* send "I'm out of here" and delete the member */
    sdt_tx_leaving(foreign_component, local_member->component, local_member , SDT_REASON_ASKED_TO_LEAVE);
    /* remove on next tick */
    local_member->expires_ms = -1;
  } else {
    acnlog(LOG_ERR | LOG_WARNING, "sdt_rx_leave: no local member");
  }
}

/*****************************************************************************/
/*
  Receive CONNECT
    local_component   - local component receiving CONNECT
    foreign_component - foreign component sending
    data              - pointer to data section of CONNECT
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_connect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t protocol;

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  UNUSED_ARG(data);

  LOG_FSTART();

  //assert(local_component);
  //assert(local_component->tx_channel);
  //assert(foreign_component);
  //assert(foreign_component->tx_channel);
  

  /* verify data length */  
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect: Invalid data_len");
    return;
  }

  protocol = unmarshalU32(data);
  
  //TODO: Implement DMP
  /*
  if (protocol == PROTO_DMP)
  {
    sdt_tx_connect_accept(wrapper, remoteLeader->component, protocol, remoteChannel);
    sdt_tx_connect(wrapper, localLeader, localChannel, remoteLeader->component, remoteChannel, PROTO_DMP);
  }
  */
}

/*****************************************************************************/
/*
  Receive CONNECT_ACCEPT
    local_component   - local component receiving CONNECT_ACCEPT
    foreign_component - foreign component sending
    data              - pointer to data section of CONNECT_ACCEPT
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  uint32_t protocol;

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  UNUSED_ARG(data);

  LOG_FSTART();

  //assert(local_component);
  //assert(local_component->tx_channel);
  //assert(foreign_component);
  //assert(foreign_component->tx_channel);

  /* verify data length */  
  if (data_len != 4) {
    acnlog(LOG_WARNING | LOG_SDT, "sdt_rx_connect_accept: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);

  //TODO wrf implment DMB
  /*
  if (protocol == PROTO_DMP) {
    acnlog(LOG_DEBUG | LOG_SDT, "sdt_rx_connect_accept: WooHoo pointless Session established");
    member->isConnected = 1;
  }
  */
}

/*****************************************************************************/
//TODO:         sdt_rx_connect_refuse();

/*****************************************************************************/
//TODO:         sdt_rx_disconnect();

/*****************************************************************************/
//TODO:         sdt_rx_disconecting();

#endif /* CONFIG_SDT */
