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
//;//syslog facility LOG_LOCAL1 is used for ACN:SDT


#include <string.h>
#include <stdio.h>

#include "opt.h"
#if CONFIG_SDT
#include "acn_arch.h"

#include "sdt.h"
#include "sdtmem.h"
#include "acn_sdt.h"
#include "rlp.h"
#include "syslog.h"
#include "slp.h"
#include "marshal.h"
#include "netiface.h"

struct netsocket_s *sdt_adhoc_socket = NULL;

uint8_t *get_pdu_buffer(void);
uint8_t *enqueue_pdu(uint16_t pduLength);
uint16_t remaining_packet_buffer(void);

//component_t *registerSdtDevice(uint8_t cid[16]);
//component_t *unregisterSdtDevice(uint8_t cid[16]);

//FIXME: need to create this function
#define timer_set(x) 0

//TODO: wrf these were moved header file for testing...
#if 0
/* BASE MESSAGES */
static void     sdt_tx_join(component_t *local_component, component_t *foreign_component);
static void     sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component);
static void     sdt_tx_join_refuse(cid_t foreign_cid, component_t *local_component, neti_addr_t *transport_addr, uint8_t *join, uint8_t reason);
static void     sdt_tx_leaving(component_t *foreign_component, component_t *local_component, uint8_t reason);
static void     sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed);

//TODO:         sdt_tx_sessions()
//TODO:         sdt_tx_get_sessions()

static void     sdt_rx_join(cid_t foreign_cid, neti_addr_t *transport_addr, uint8_t *data, uint32_t data_len);
static void     sdt_rx_join_accept(cid_t foreign_cid, uint8_t *join_accept, uint32_t data_len);
static void     sdt_rx_join_refuse(cid_t foreign_cid, uint8_t *join_refuse, uint32_t data_len);
static void     sdt_rx_leaving(cid_t foreign_cid, uint8_t *leaving, uint32_t data_len);
static void     sdt_rx_nak(cid_t foreign_cid, uint8_t *nak, uint32_t data_len);
static void     sdt_rx_wrapper(cid_t foreign_cid, neti_addr_t *transport_addr, uint8_t *wrapper, uint32_t data_len);
//TODO:         sdt_rx_get_sessions()
//TODO:         sdt_rx_sessions()

/* WRAPPED MESSAGES */
static void     sdt_tx_ack(component_t *local_component, component_t *foreign_component);
//TODO:         sdt_tx_channel_params(sdt_wrapper_t *wrapper);
static void     sdt_tx_leave(component_t *local_component, component_t *foreign_component);
static void     sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
static void     sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_connect_refuse(sdt_wrapper_t *wrapper);
static void     sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_disconecting(sdt_wrapper_t *wrapper);

static void     sdt_rx_ack(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_channel_params();
static void     sdt_rx_leave(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len);
static void     sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_connect_refuse();
//TODO:         sdt_rx_disconnect();
//TODO:         sdt_rx_disconecting();

#endif

/* UTILITY FUNCTIONS */


static int      pack_channel_param_block(uint8_t *paramBlock);
static int      unpack_channel_param_block(sdt_member_t *member, uint8_t *param_block);

static int      pack_transport_address(uint8_t *data, neti_addr_t *transport_addr, int type);
static int      unpack_transport_address(uint8_t *data, neti_addr_t *transport_addr, neti_addr_t *packet_addr, uint8_t *type);

static void     sdt_tick(void);
static void     sdt_client_rx_handler(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len);

static uint8_t *sdt_format_client_block(uint8_t *client_block, uint16_t foreignlMember, uint32_t protocol, uint16_t association);

static uint8_t *sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold);

static uint32_t check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail);

enum
{
  SDT_STATE_NON_MEMBER,
  SDT_STATE_JOIN_SENT,
  SDT_STATE_JOIN_ACCEPT_SENT,
  SDT_STATE_JOIN_RECEIVED,
  SDT_STATE_CHANNEL_OPEN,
  SDT_STATE_LEAVING,  
};

/* may go away, used for testing */
int sdt_get_adhoc_port(void)
{
  return NSK_PORT(*sdt_adhoc_socket);
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
sdt_rx_handler(uint8_t *data, int data_len, void *ref, neti_addr_t *remhost, cid_t foreign_cid)
{
  uint8_t    *data_end;
  uint8_t     vector   = 0;
	uint32_t    data_size = 0;
	uint8_t    *pdup, *datap;

  UNUSED_ARG(ref);
  
  /* local pointer */
	pdup = data;

  /* verify min data length */
  if (data_len < 3) {
		syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* On first packet, flags should all be set */
  // TODO: support for long packets?
	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_handler: illegal first PDU flags");
		return;
	}

  /* Process all PDU's in root layer PDU*/
  while(pdup < data_end)  {
		uint8_t  flags;
		uint8_t *pp;

    /* Decode the sdt base layer header, getting flags, length and vector */
    flags = unmarshalU8(pdup);

    /* save pointer, on second pass, we don't know if this is vector or data until we look at flags*/
		pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);
    /* fail if outside our packet */
		if (pdup >= data_end) {
			syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_handler: packet length error");
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
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_join");
        sdt_rx_join(foreign_cid, remhost, datap, data_size);
        break;
      case SDT_JOIN_ACCEPT :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_join_accept");
        sdt_rx_join_accept(foreign_cid, datap, data_size);
        break;
      case SDT_JOIN_REFUSE :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_join_refuse");
        sdt_rx_join_refuse(foreign_cid, datap, data_size);
        break;
      case SDT_LEAVING :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_leaving");
        sdt_rx_leaving(foreign_cid, datap, data_size);
        break;
      case SDT_NAK :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_nak");
        sdt_rx_nak(foreign_cid, datap, data_size);
        break;
      case SDT_REL_WRAPPER :
      case SDT_UNREL_WRAPPER :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Dispatch to sdt_rx_wrapper");
        sdt_rx_wrapper(foreign_cid, remhost, datap, data_size);
        break;
      case SDT_GET_SESSIONS :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Get Sessions unsupported");
        break;
      case SDT_SESSIONS :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdtRxHandler: Sessions??? I don't want any of that !");
        break;
      default:
        syslog(LOG_WARNING | LOG_LOCAL1,"sdtRxHandler: Unknown Vector (protocol) - skipping");
    } /* switch */
  } /* while() */
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_handler: End");
  return;
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

  returns pointer to start of client block
*/
/*static*/ uint8_t *
sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold)
{
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_format_wrapper");

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
  wrapper += marshalU8(wrapper, (is_reliable) ? SDT_REL_WRAPPER : SDT_UNREL_WRAPPER);
  /* channel number */
  wrapper += marshalU16(wrapper, local_channel->number);
  wrapper += marshalU32(wrapper, local_channel->total_seq);
  wrapper += marshalU32(wrapper, local_channel->reliable_seq);

  //FIXME - Change to oldest available when buffering support added
  wrapper += marshalU32(wrapper, local_channel->oldest_avail); 
  wrapper += marshalU16(wrapper, first_mid);
  wrapper += marshalU16(wrapper, last_mid);
  wrapper += marshalU16(wrapper, mak_threshold);

  /* return pointer to client block */
  return wrapper;
}


/*****************************************************************************/
/*static*/ void 
sdt_client_rx_handler(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len)
{
  uint8_t    *data_end;
  uint8_t     vector   = 0;
	uint32_t    data_size = 0;
	uint8_t    *pdup, *datap;
            
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler");

  /* verify min data length */
  if (data_len < 3) {
		syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* start of our pdu block containing one or multiple pdus*/
  pdup = data;

	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		syslog(LOG_ERR | LOG_LOCAL1,"sdt_client_rx_handler: illegal first PDU flags");
		return;
	}

  while(pdup < data_end)  {
		uint8_t  flags;
		uint8_t *pp;

    /* Decode flags */
    flags = unmarshalU8(pdup);

    /* save pointer*/
		pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);
    
		if (pdup >= data_end) {
			syslog(LOG_ERR | LOG_LOCAL1,"sdt_client_rx_handler: packet length error");
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
        //remoteMember = sdtm_find_member_by_component(localChannel, remoteLeader->component);
        //if (!remoteMember) {
        //  syslog(LOG_ERR | LOG_LOCAL1,"sdt_client_rx_handler: Can't find remoteMember");
        //  return;
        //}
        sdt_rx_ack(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CHANNEL_PARAMS :
        break;
      case SDT_LEAVE :
        sdt_rx_leave(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT :
        sdt_rx_connect(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT_ACCEPT :
        //remoteMember = sdtm_find_member_by_component(localChannel, remoteLeader->component);
        //if (!remoteMember) {
        //  syslog(LOG_ERR | LOG_LOCAL1,"sdt_client_rx_handler: Can't find remoteMember");
        //  return;
        //}
        sdt_rx_connect_accept(local_component, foreign_component, pdup, data_size);
        break;
      case SDT_CONNECT_REFUSE :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler: Our Connect was Refused ??? ");
        break;
      case SDT_DISCONNECT :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler: rx sdt DISCONNECT (NOP)");
        break;
      case SDT_DISCONNECTING :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler: rx sdt DISCONNECTING (isn't that special)");
        break;
      default :
        syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler: Unknown Vector -- Skipping");
    }
  }
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_client_rx_handler End");
}

/*****************************************************************************/
/* returns SEQ_VALID if packet is correctly sequenced */
/*static*/ uint32_t 
check_sequence(sdt_channel_t *channel, bool is_reliable, uint32_t total_seq, uint32_t reliable_seq, uint32_t oldest_avail)
{
  int diff;
  
  diff = total_seq - channel->total_seq;
//  syslog(LOG_WARNING | LOG_LOCAL1, "sdtCheckSequence: Rel?=%d, channel->tot=%d, channel->rel=%d, rxTot=%d, rxRel=%d, oldest_avail=%d",
//        is_reliable, channel->total_seq, channel->reliable_seq, total_seq, reliable_seq, oldest_avail);

  if (diff == 1) { 
    /* normal seq */
    syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: ok"); 
    /* update channel object */
    channel->total_seq = total_seq;
    channel->reliable_seq = reliable_seq;
    return SEQ_VALID; /* packet should be processed */
  } else {
    if (diff > 1) { 
      /* missing packet(s) check reliable */
      diff = reliable_seq - channel->reliable_seq;
      if ((is_reliable) && (diff == 1))  {
        syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: missing unreliable");
        /* update channel object */
        channel->total_seq = total_seq;
        channel->reliable_seq = reliable_seq;
        return SEQ_VALID;
      } else {
        if (diff) {
          /* missed reliable packets */
          diff = oldest_avail - channel->reliable_seq;
          if (diff > 1) {
            syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: sequence lost");
            return SEQ_LOST; 
          } else {
            syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: sequence nak");            
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
        syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: duplicate");
        return SEQ_DUPLICATE;
      /* else diff < 0 */
      syslog(LOG_DEBUG | LOG_LOCAL1, "check_sequence: old packet");
      return SEQ_OLD_PACKET;
    }
  } 
}

enum
{
  ALLOCATED_FOREIGN_COMP = 1,
  ALLOCATED_FOREIGN_LEADER = 2,
  ALLOCATED_FOREIGN_CHANNEL = 4,
  ALLOCATED_LOCAL_MEMBER = 8,
  ALLOCATED_LOCAL_LEADER = 16,
  ALLOCATED_LOCAL_CHANNEL = 32,
  ALLOCATED_FOREIGN_MEMBER = 64,
};

component_t *my_component;
cid_t my_xcid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
cid_t my_xdcid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*static*/ void
sdt_local_init(void)
{
  my_component = sdtm_add_component(my_xcid, my_xdcid, true);
  if (!my_component) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_local_init : failed to get new component");
  return;
  }

}

/*****************************************************************************/
/*
  Send JOIN
  local_comonent     - local component with pre-assigned tx_channel
  foreign_compoinent - foreign component
*/
void 
sdt_tx_join(component_t *local_component, component_t *foreign_component)
{
  sdt_member_t  *foreign_member;
  sdt_channel_t *local_channel;
  uint8_t *buf_start;
  uint8_t *buffer;

  rlp_txbuf_t *tx_buffer;

  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_tx_join");

  /* just to make it easier */
  local_channel = local_component->tx_channel;

  /* Sanity check  */
  foreign_member = sdtm_find_member_by_component(local_channel, foreign_component);
  if (foreign_member) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_tx_join : already a member -- suppressing join");
    return; /* Dont add them twice. */
  }  

  /* put foreign component in our channel list */
  foreign_member = sdtm_add_member(local_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_join : failed to allocate foreign member");
    return;
  }
  foreign_member->mid = sdtm_next_member(local_channel);
  /* Timeout if JOIN ACCEPT is not received */
  foreign_member->expires_at = timer_set(FOREIGN_MEMBER_EXPIRY_TIME);

  /* Create packet buffer*/
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_join : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  // TODO: add unicast/multicast support
//  buffer += marshalU16(buffer, 49 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer += marshalU16(buffer, 43 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer += marshalU8(buffer, SDT_JOIN);
  buffer += marshalUUID(buffer, foreign_component->cid);
  buffer += marshalU16(buffer, foreign_member->mid);
  buffer += marshalU16(buffer, local_channel->number);
  if (foreign_component->tx_channel)
    buffer += marshalU16(buffer, foreign_component->tx_channel->number); /* Reciprocal Channel */
  else
    buffer += marshalU16(buffer, 0);                      /* Reciprocal Channel */
  buffer += marshalU32(buffer, local_channel->total_seq);
  buffer += marshalU32(buffer, local_channel->reliable_seq);
  
  // TODO: add unicast/multicast support. 0 is UNICAST
  buffer += marshalU8(buffer, 0);
//  buffer += pack_transport_address(buffer, lChannel->downstreamPort, lChannel->downstreamIP, SDT_ADDR_IPV4);
  buffer += pack_channel_param_block(buffer);
  buffer += marshalU8(buffer, 255); //My Adhoc Port does not change

  /* JOINS are always sent to foreign compoents adhoc address */
  /* and from a adhoc address                                 */
//  rlp_add_pdu(tx_buffer, buf_start, 49, NULL);
  rlp_add_pdu(tx_buffer, buf_start, 43, NULL);
  rlp_send_block(tx_buffer, sdt_adhoc_socket, &foreign_component->adhoc_addr);
  rlpm_freetxbuf(tx_buffer);
}


/*****************************************************************************/
/*
  Send JOIN_ACCEPT
    local_member      - local member
    local_component   - local component
    foreign_component - foreign component
*/
/*static*/ void 
sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *tx_buffer;

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_join_accept");

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_join_accept : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(tx_buffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer += marshalU16(buffer, 29 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer += marshalU8(buffer, SDT_JOIN_ACCEPT);
  /* data */
  buffer += marshalUUID(buffer, foreign_component->cid);
  buffer += marshalU16(buffer, foreign_component->tx_channel->number);
  buffer += marshalU16(buffer, local_member->mid);
  buffer += marshalU32(buffer, foreign_component->tx_channel->reliable_seq);
  buffer += marshalU16(buffer, local_component->tx_channel->number);

  /* add our PDU */
  rlp_add_pdu(tx_buffer, buf_start, 29, NULL);
  /* JOIN_ACCEPT are sent address where the JOIN came from */
  /* this should be the foreign componets adhoc address    */
  rlp_send_block(tx_buffer, foreign_component->tx_channel->sock, &foreign_component->adhoc_addr);
}

/*****************************************************************************/
/*
  Send JOIN_REFUSE
    foreign_cid     - cid of component we are sendign this to
    local_component - component sending 
    transport_addr  - address join message originated from
    join            - pointer to "data" of original JOIN message
    reason         - reason we are refusing to join
*/
/*static*/ void 
sdt_tx_join_refuse(cid_t foreign_cid, component_t *local_component, neti_addr_t *transport_addr, uint8_t *join, uint8_t reason)
{
  uint8_t *buf_start;
  uint8_t *buffer;
  rlp_txbuf_t *txBuffer;

  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_tx_join_refuse");

  /* Create packet buffer */
  txBuffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!txBuffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_join_refuse : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(txBuffer, NULL);
  buffer = buf_start;

  /* length and flags */
  buffer += marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  /* vector */
  buffer += marshalU8(buffer, SDT_JOIN_REFUSE);
  /* data */
  buffer += marshalUUID(buffer, foreign_cid);
  buffer += marshalU16(buffer, unmarshalU16(join + 18)); /* leader's channel # */
  buffer += marshalU16(buffer, unmarshalU16(join + 16)); /* mid that the leader was assigning to me */
  buffer += marshalU32(buffer, unmarshalU32(join + 26)); /* leader's channel rel seq # */
  buffer += marshalU8(buffer, reason);
  
  /* add our PDU */
  rlp_add_pdu(txBuffer, buf_start, 28, NULL);

  /* send via our ad-hoc to address it came from */
  rlp_send_block(txBuffer, sdt_adhoc_socket, transport_addr);
}

/*****************************************************************************/
/*
  Send LEAVING
*/
/*static*/ void 
sdt_tx_leaving(component_t *foreign_component, component_t *local_component, uint8_t reason)
{
  uint8_t       *buf_start;
  uint8_t       *buffer;
  rlp_txbuf_t   *txBuffer;
  sdt_member_t  *local_member;
  sdt_channel_t *foreign_channel;

  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_tx_leaving");

  if (!foreign_component->tx_channel) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_leaving : foreign component without channel");
    return;
  }
  foreign_channel = foreign_component->tx_channel;

  local_member = sdtm_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_leaving : failed to find local_member");
  }

  /* Create packet buffer */
  txBuffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!txBuffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_leaving : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(txBuffer, NULL);
  buffer = buf_start;

  buffer += marshalU16(buffer, 28 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer += marshalU8(buffer, SDT_LEAVING);
  buffer += marshalUUID(buffer, foreign_component->cid);
  buffer += marshalU16(buffer, foreign_channel->number);
  buffer += marshalU16(buffer, local_member->mid);
  buffer += marshalU32(buffer, foreign_channel->reliable_seq);
  buffer += marshalU8(buffer, reason);

  /* add our PDU */
  rlp_add_pdu(txBuffer, buf_start, 28, NULL);

  /* send via channel */
  rlp_send_block(txBuffer,foreign_channel->sock, foreign_channel->downstream);
}

/*****************************************************************************/
/*
  Send NAK
*/
/*static*/ void 
sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed)
{
  uint8_t       *buf_start;
  uint8_t       *buffer;
  rlp_txbuf_t   *txBuffer;
  sdt_member_t  *local_member;
  sdt_channel_t *foreign_channel;
  
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_nak");

  if (!foreign_component->tx_channel) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_nak : foreign component without channel");
    return;
  }
  foreign_channel = foreign_component->tx_channel;

  local_member = sdtm_find_member_by_component(foreign_channel, local_component);
  if (!local_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_nak : failed to find local_member");
  }

  /* Create packet buffer */
  txBuffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!txBuffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_nak : failed to get new txbuf");
    return;
  } 
  buf_start = rlp_init_block(txBuffer, NULL);
  buffer = buf_start;

  //FIXME -  add proper nak storm suppression
  
  buffer += marshalU16(buffer, 35 /* Length of this pdu */ | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  buffer += marshalU8(buffer, SDT_NAK);
  buffer += marshalUUID(buffer, foreign_component->cid);
  buffer += marshalU16(buffer, foreign_channel->number);
  buffer += marshalU16(buffer, local_member->mid);
  buffer += marshalU32(buffer, foreign_channel->reliable_seq);       /* last known good one */
  buffer += marshalU32(buffer, foreign_channel->reliable_seq + 1);   /* then the next must be the first I missed */
  buffer += marshalU32(buffer, last_missed);

  /* add our PDU */
  rlp_add_pdu(txBuffer, buf_start, 35, NULL);

  /* send via channel */
  // TODO: look at channel parameter NAK (see 4.2.5)
  rlp_send_block(txBuffer, foreign_channel->sock, foreign_channel->downstream);
}

//TODO:         sdt_tx_sessions()
//TODO:         sdt_tx_get_sessions()

/*****************************************************************************/
/*
  Received JOIN message
    foreign_cid    - cid of component that sent the message
    transport_addr - address JOIN message was received from
    join           - points to DATA section of JOIN message
    data_len       - length of DATA section.
*/
/*static*/ void
sdt_rx_join(cid_t foreign_cid, neti_addr_t *transport_addr, uint8_t *join, uint32_t data_len)
{
  cid_t     local_cid;
  uint8_t  *joinp;
  uint16_t  foreign_channel_number;
  uint16_t  local_channel_number;
  
  uint16_t         local_mid;
  component_t     *local_component;
  component_t     *foreign_component;

  sdt_channel_t   *foreign_channel;
  sdt_member_t    *local_member;
  
  uint8_t     address_type;
//  int       allocations = 0;    /*  flags so we can undo */

  syslog(LOG_DEBUG |LOG_LOCAL1, "sdt_rx_join");

  /* verify data length */  
  if (data_len < 40) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join: pdu too short");
    return;
  }

  /* get local pointer */
  joinp = join;

  /* destination cid */
  unmarshalUUID(joinp, local_cid);
  joinp += UUIDSIZE;

  /* see if this is for one of our components */
  local_component = sdtm_find_component(local_cid);
  if (!local_component) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join: Not addressed to me");
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

  /* if we have reciprocal, then in theory, the channel on this end should have been created */
  if (local_channel_number) {
    if (!local_component->tx_channel) {
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join: local component without channel");
      sdt_tx_join_refuse(foreign_cid, local_component, transport_addr, join, SDT_REASON_NONSPEC);
      return;
    }
    if (local_component->tx_channel->number != local_channel_number) {
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join: invalid reciprocal channel number");
      sdt_tx_join_refuse(foreign_cid, local_component, transport_addr, join, SDT_REASON_NO_RECIPROCAL);
      return;
    }
  }

  // TODO: possible callback to app to verify accept
  
  /*  Are we already tracking the src component, if not add it */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component) {
    foreign_component = sdtm_add_component(foreign_cid, NULL, false);
    if (!foreign_component) { /* allocation failure */
      syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join: failed to add foreign_component");
      sdt_tx_join_refuse(foreign_cid, local_component, transport_addr, join, SDT_REASON_RESOURCES);
      return;
//    } else {
//      allocations |= ALLOCATED_FOREIGN_COMP;
    }  
  }

  
  /* foreign channel should not exist yet! */
  if (foreign_component->tx_channel) {
      syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join: pre-existing foreign channel");
  }

  /* add foreign channel */
  foreign_channel = sdtm_add_channel(foreign_component, foreign_channel_number, 0);
  //TODO: add channel parameters
  //foreign_channel->sock...
  //foreign_channel
  //foreign_channel
  /* Join comes from foreign adhoc address */
  /* if we already have this component, we should already have this address, but just in case */
  //foreignComp->adhoc_addr.addr = transport_addr->addr;
  //foreignComp->adhoc_addr.port = transport_addr->port;
      //mcast_join(localChannel->downstreamIP);  // DEPENDS WATERLOO
      //allocations |= ALLOCATED_LOCAL_CHANNEL;

  joinp += unpack_transport_address(joinp, transport_addr, foreign_channel->downstream, &address_type);

  if (address_type == SDT_ADDR_IPV6) {
    syslog(LOG_WARNING | LOG_LOCAL1, "sdt_rx_join: Unsupported downstream Address Type");
    return;
    //send refuse -- reason ADDR_TYPE - txRefuse
    //FIXME --handle error - txRefuse
  }

  /* fill in the channel structure */
  foreign_channel->total_seq = unmarshalU32(joinp);
  joinp += sizeof(uint32_t);
  foreign_channel->reliable_seq = unmarshalU32(joinp);
  joinp += sizeof(uint32_t);

  /* add local component to foreign channel */
  local_member = sdtm_add_member(foreign_channel, local_component);
  if (!local_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join: failed to add local member");
    sdt_tx_join_refuse(foreign_cid, local_component, transport_addr, join, SDT_REASON_RESOURCES);
    return;
  }
  local_member->mid = local_mid;
  //TODO: Get real values
  local_member->expiry_time = 0;//TIMEOUT;

  /* fill in the member structure */
  joinp += unpack_channel_param_block(local_member, joinp);
  
  //if (allocations & ALLOCATED_FOREIGN_CHANNEL)   {
  //  foreignChannel->sock = rlp_open_netsocket(foreignChannel->downstream.port);
  //  //FIXME: wrf, comment out for now
  //  //mcast_join(foreignChannel->downstreamIP);  // DEPENDS WATERLOO
  //}

  local_member->pending = 1;
  local_member->expires_at = timer_set(local_member->expiry_time);
  sdt_tx_join(local_component, foreign_component);

  if (!local_channel_number) {
    sdt_tx_join_accept(local_member, local_component, foreign_component);
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
sdt_rx_join_accept(cid_t foreign_cid, uint8_t *join_accept, uint32_t data_len)
{
  component_t     *foreign_component;
  uint16_t         foreign_channel_number;
  uint16_t         foreign_mid;
  component_t     *local_component;
  cid_t            local_cid;
  uint16_t         local_channel_number;
  uint32_t         rel_seq_number;
  
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_join_accept");

  /* verify data length */
  if (data_len != 26) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_accept: invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_accept: foreign_component not found");
    return;
  }

  /* verify leader CID */
  unmarshalUUID(join_accept, local_cid);
  join_accept += UUIDSIZE;
  local_component = sdtm_find_component(local_cid);
  if (!local_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_accept: not for me");
    return;
  }

  /* sanity check - local component should always have a channel */
  //if (!local_component->tx_channel) {
  //  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join_accept: local component without channel");
  //  return;
  //}

  /* verify we have a matching channel number */
  local_channel_number = unmarshalU8(join_accept);
  join_accept += sizeof(uint16_t);
  if (local_component->tx_channel->number != local_channel_number) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join_accept: local channel number mismatch");
    return;
  }

  /* verify MID */
  foreign_mid = unmarshalU16(join_accept);
  join_accept += sizeof(uint16_t);
  if (!sdtm_find_member_by_mid(local_component->tx_channel, foreign_mid)) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join_accept: MID not found");
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
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_join_accept: foreign channel number mismatch");
      return;
    }
  }
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
sdt_rx_join_refuse(cid_t foreign_cid, uint8_t *join_refuse, uint32_t data_len)
{
  cid_t             local_cid;
  component_t      *local_component;
  uint16_t          local_channel_number;
  component_t      *foreign_component;
  uint16_t          foreign_mid;
//  sdt_member_t     *foreign_member;
  uint32_t          rel_seq_num;
  uint8_t           reason_code;

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_join_refuse");

  /* verify data length */
  if (data_len != 23) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_refuse: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_refuse: foreign_component not found");
    return;
  }

  /* get leader */
  unmarshalUUID(join_refuse, local_cid);
  join_refuse += UUIDSIZE;
  local_component = sdtm_find_component(local_cid);
  if (!local_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_refuse: Not for me");
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
  syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_join_refuse: reason.xxx");

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
sdt_rx_leaving(cid_t foreign_cid, uint8_t *leaving, uint32_t data_len)
{
  component_t     *foreign_component;
  component_t     *local_component;
  cid_t            local_cid;
  uint16_t         local_channel_number;
  uint16_t         foreign_mid;
  sdt_member_t    *foreign_member;


  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_leaving");

  /* verify data length */
  if (data_len != 25) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_leaving: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_leaving: foreign_component not found");
    return;
  }

  /* get leader cid */
  unmarshalUUID(leaving, local_cid);
  leaving += UUIDSIZE;
  local_component = sdtm_find_component(local_cid);
  if (!local_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_leaving: local_component not found");
    return;
  }

  /* get channel number */
  local_channel_number = unmarshalU16(leaving);
  leaving += sizeof(uint16_t);
  if (!local_component->tx_channel) {
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_leaving: local component without channel");
      return;
  }
  if (local_component->tx_channel->number != local_channel_number) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_leaving: local channel number mismatch");
    return;
  }

  foreign_mid = unmarshalU16(leaving);
  leaving += sizeof(uint16_t);
  foreign_member = sdtm_find_member_by_mid(local_component->tx_channel, foreign_mid);
  if (!foreign_member)  {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_leaving: member not found");
    return;
  }

  // TODO: implement DMP or callback

  /* ok now remove it */
  sdtm_remove_member(local_component->tx_channel, foreign_member);

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
sdt_rx_nak(cid_t foreign_cid, uint8_t *nak, uint32_t data_len)
{
  cid_t            local_cid;
  component_t      *local_component;
  uint16_t         local_channel_number;
  component_t     *foreign_component;
   
  uint32_t        first_missed;
  uint32_t        last_missed;
  int             num_back;
  
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_nak");

  /* verify data length */
  if (data_len != 32) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_nak: Invalid data_len");
    return;
  }

  /* verify we are tracking this component */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_nak: foreign_component not found");
    return;
  }
 
  /* get Leader CID */
  unmarshalUUID(nak, local_cid);
  nak += UUIDSIZE;
  local_component = sdtm_find_component(local_cid);
  if (!local_component) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_nak: local_component not found");
    return;
  }
  
  local_channel_number = unmarshalU16(nak);
  nak += sizeof(uint16_t);
  if (!local_component->tx_channel) {
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_nak: local component without channel");
      return;
  }
  if (local_component->tx_channel->number != local_channel_number) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_nak: local channel number mismatch");
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
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_nak: Requested Wrappers not avail");
    return;
  }
  
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
    transport_addr - address wrapper came from
    wrapper        - pointer to wrapper DATA block
    data_len       - length of data block
*/
/*static*/ void
sdt_rx_wrapper(cid_t foreign_cid, neti_addr_t *transport_addr, uint8_t *wrapper, uint32_t data_len)
{
  component_t      *foreign_component;
  sdt_channel_t   *foreign_channel;
  sdt_member_t    *local_member;
  
  uint8_t    *wrapperp;

  uint8_t    *data_end;
	uint32_t    data_size = 0;
	uint8_t    *pdup, *datap;

  uint16_t    local_mid = 0;
  uint32_t    protocol = 0;
  uint16_t    association = 0;
  
  bool     is_reliable;
  uint16_t channel_number;
  uint32_t total_seq;
  uint32_t reliable_seq;
  uint32_t oldest_avail;
  uint16_t first_mid;
  uint16_t last_mid;
  uint16_t mak_threshold;

  UNUSED_ARG(transport_addr);

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_wrapper");

  /* verify length */
  if (data_len < 23) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_wrapper: Invalid data_len");
    return;
  }
  data_end = wrapper + data_len;

  /* verify we are tracking this component */
  foreign_component = sdtm_find_component(foreign_cid);
  if (!foreign_component)  {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_wrapper: Not tracking this component");
    return;
  }

  if (!foreign_component->tx_channel) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_wrapper : foreign component without channel");
    return;
  }

  foreign_channel = foreign_component->tx_channel;

  /* local pointer */
  wrapperp = wrapper;

  /* skip length, flags */
  wrapperp += sizeof(uint16_t);

  /* reliable? */
  is_reliable = (unmarshalU8(wrapperp) == SDT_REL_WRAPPER) ? true : false;
  wrapperp += sizeof(uint16_t);
  
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
      syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_wrapper: Sequence correct");
      break; /* continue on with this packet */
    case SEQ_NAK :
      /* initiate nak processing */
      syslog(LOG_WARNING | LOG_LOCAL1, "sdt_rx_wrapper: Missing wrapper(s) detected - Sending NAK");
      local_member = foreign_channel->member_list;
      while (local_member) {
        sdt_tx_nak(foreign_component, local_member->component, reliable_seq);
        local_member = local_member->next;
      }
      return;
      break;
    case SEQ_OLD_PACKET :
      syslog(LOG_INFO | LOG_LOCAL1, "sdt_rx_wrapper: Old (Out of sequence) wrapper detected - Discarding");
      /* Discard */
      return;
      break;
    case SEQ_DUPLICATE :
      syslog(LOG_INFO | LOG_LOCAL1, "sdt_rx_wrapper: Duplicate wrapper detected - Discarding");
      /* Discard */
      return;      
      break;
    case SEQ_LOST : 
      syslog(LOG_INFO | LOG_LOCAL1, "sdt_rx_wrapper: Lost Reliable Wrappers not available - LOST SEQUENCE");
      /* Discard and send sequence lost message to leader */
      local_member = foreign_channel->member_list;
      while (local_member) {
        sdt_tx_leaving(foreign_component, local_member->component, SDT_REASON_LOST_SEQUENCE);
        local_member = local_member->next;
      }
      return;
      break;
  }

  //TODO: Currently doesn't care about the MAK Threshold

  /* deal with pending state */
  local_member = foreign_channel->member_list;
  while (local_member) {
    if (local_member->pending || ((local_member->mid >= first_mid) && (local_member->mid <= last_mid)))  {
      local_member->pending = 0;
      sdt_tx_ack(local_member->component, foreign_component);
    }
    /* reset the timeout */
    local_member->expires_at = timer_set(local_member->expiry_time); 
    local_member = local_member->next;
  }

  /* start of our client block containing one or multiple pdus*/
  pdup = wrapperp;

	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_wrapper: illegal first PDU flags");
		return;
	}

  /* decode and dispatch the wrapped pdus (Wrapped SDT or DMP) */
  while(pdup < data_end)  {
		uint8_t  flags;
		uint8_t *pp;

    flags = unmarshalU8(pdup);
  
    /* save pointer, on second pass, we don't know if this is vector, header or data until we look at flags*/
		pp = pdup + 2;

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);
    /* fail if outside our packet */
		if (pdup >= data_end) {
			syslog(LOG_ERR | LOG_LOCAL1,"sdt_rx_wrapper: packet length error");
			return;
		}

    /* Get vector or leave the same as last = MID*/
    if (flags & VECTOR_bFLAG) {
      local_mid = unmarshalU8(pp); 
			pp += sizeof(uint8_t);
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
          if (local_member->component->tx_channel->number != association) {
            syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_wrapper: association channel not found");
            return;
          }
        }
        if (protocol == PROTO_SDT) {
          sdt_client_rx_handler(local_member->component, foreign_component, pdup, data_size);
        } //else {
//        rx_handler = sdt_get_rx_handler(clientProtocol,ref)    
//        if(!rx_handler) {
//          syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_wrapper: Unknown Vector - skip");
//        } else {
//          rx_handler(remoteLeader->component,member->component,
//          remoteChannel, clientPdu.data, clientPdu.dataLength, ref);
//        }        
//      }
      }
      local_member = local_member->next;
    }
  }

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_wrapper: Complete");
  return;
}

//TODO:         sdt_rx_get_sessions()
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

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_ack");

  /* get local member in foreign channel */
  foreign_member = sdtm_find_member_by_component(local_component->tx_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_ack : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_ack : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_channel = foreign_component->tx_channel;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_UNRELIABLE, local_component->tx_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (ACK message) */
  datagram += marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram += marshalU8(datagram, SDT_ACK);
  datagram += marshalU32(datagram, foreign_channel->reliable_seq);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer,foreign_channel->sock, foreign_channel->downstream);
}

//TODO:         sdt_tx_channel_params();

/*****************************************************************************/
/*
  Send LEAVE
    local_component   - 
    foreign_component -

  LEAVE is sent in a reliable wrapper
*/
/*static*/ void 
sdt_tx_leave(component_t *local_component, component_t *foreign_component)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_leave");

  /* get local member in foreign channel */
  foreign_member = sdtm_find_member_by_component(local_component->tx_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_leave : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_leave : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_channel = foreign_component->tx_channel;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_component->tx_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (LEAVE message) */
  datagram += marshalU16(datagram, 3 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram += marshalU8(datagram, SDT_LEAVE);

  /* add length to client block pdu */
  marshalU16(client_block, (10+3) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer,foreign_channel->sock, foreign_channel->downstream);
}

/*****************************************************************************/
/*
  Send CONNECT
    local_component   - 
    foreign_component -
    protocol          -

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

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_connect");

  /* get local member in foreign channel */
  foreign_member = sdtm_find_member_by_component(local_component->tx_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_connect : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_connect : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_channel = foreign_component->tx_channel;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_component->tx_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram CONNECT message) */
  datagram += marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram += marshalU8(datagram, SDT_CONNECT);
  datagram += marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer,foreign_channel->sock, foreign_channel->downstream);
}

/*****************************************************************************/
/*
  Send CONNECT_ACCEPT
    local_component   - 
    foreign_component -
    protocol          -

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

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_connect_accept");

  /* get local member in foreign channel */
  foreign_member = sdtm_find_member_by_component(local_component->tx_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_connect_accept : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_connect_accept : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_channel = foreign_component->tx_channel;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_component->tx_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (CONNECT_ACCEPT message) */
  datagram += marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram += marshalU8(datagram, SDT_CONNECT_ACCEPT);
  datagram += marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer,foreign_channel->sock, foreign_channel->downstream);
}

//TODO:         sdt_tx_connect_refuse();

/*****************************************************************************/
/*
  Send DISCONNECT
    local_component   - 
    foreign_component -
    protocol          -

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

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_tx_disconnect");

  /* get local member in foreign channel */
  foreign_member = sdtm_find_member_by_component(local_component->tx_channel, foreign_component);
  if (!foreign_member) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_disconnect : failed to get foreign_member");
    return;
  }
    
  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_tx_disconnect : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_channel = foreign_component->tx_channel;

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, SDT_RELIABLE, local_component->tx_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */
  
  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_SDT, foreign_channel->number);

  /* add datagram (DISCONNECT message) */
  datagram += marshalU16(datagram, 7 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram += marshalU8(datagram, SDT_DISCONNECT);
  datagram += marshalU32(datagram, protocol);

  /* add length to client block pdu */
  marshalU16(client_block, (10+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);
  /* and send it on */
  rlp_send_block(tx_buffer,foreign_channel->sock, foreign_channel->downstream);
}

//TODO:         sdt_tx_disconecting();

/*****************************************************************************/
/*
  Receive ACK
    local_component   - 
    foreign_component -
    data              - pointer to data section of ACK
    data_len          - length of data section

*/
/*static*/ void 
sdt_rx_ack(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len)
{
  uint32_t reliableSeq;

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  
  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_ack");

  /* verify data length */
  if (data_len != 4) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_ack: Invalid data_len");
    return;
  }

  reliableSeq = unmarshalU32(data);
  
  //remoteMember->expiresAt = timer_set(FOREIGN_MEMBER_EXPIRY_TIME);
  
  return;
}

//TODO:         sdt_rx_channel_params();

/*****************************************************************************/
/*
  Receive LEAVE
    local_component   - 
    foreign_component -
    data              - pointer to data section of LEAVE
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_leave(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len)
{

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  UNUSED_ARG(data);

  syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_leave");

  /* verify data length */  
  if (data_len != 0) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_leave: Invalid data_len");
    return;
  }

  //sdt_member_t *member = sdtm_find_member_by_component(remoteChannel, localLeader->component);
  
  //sdt_tx_leaving(remoteLeader, remoteChannel, member, SDT_REASON_ASKED_TO_LEAVE);
  // wrf - add support for jiffies?
  //member->expiresAt = 0;//jiffies; //expire the member
  //syslog(LOG_DEBUG | LOG_LOCAL1, "rxLeave I'm feeling better");
}

/*****************************************************************************/
/*
  Receive CONNECT
    local_component   - 
    foreign_component -
    data              - pointer to data section of CONNECT
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_connect(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len)
{
  uint32_t protocol;

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  UNUSED_ARG(data);
  
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_connect");

  /* verify data length */  
  if (data_len != 4) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_connect: Invalid data_len");
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
    local_component   - 
    foreign_component -
    data              - pointer to data section of CONNECT_ACCEPT
    data_len          - length of data section
*/
/*static*/ void 
sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, uint8_t *data, uint32_t data_len)
{
  uint32_t protocol;

  UNUSED_ARG(local_component);
  UNUSED_ARG(foreign_component);
  UNUSED_ARG(data);

  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_rx_connect_accept");

  /* verify data length */  
  if (data_len != 4) {
    syslog(LOG_ERR | LOG_LOCAL1, "sdt_rx_connect_accept: Invalid data_len");
    return;
  }

  /*  verify protocol */
  protocol = unmarshalU32(data);

  //TODO wrf implment DMB
  /*
  if (protocol == PROTO_DMP) {
    syslog(LOG_DEBUG | LOG_LOCAL1, "sdt_rx_connect_accept: WooHoo pointless Session established");
    member->isConnected = 1;
  }
  */
}

//TODO:         sdt_rx_connect_refuse();
//TODO:         sdt_rx_disconnect();
//TODO:         sdt_rx_disconecting();

// utility Functions

/*****************************************************************************/
/*-----------------------------------------------------------------------------
 Format a clinet block into the currrent SDT wrapper.  
 You must call sdt_format_wrapper first
 -----------------------------------------------------------------------------*/
uint8_t *
sdt_format_client_block(uint8_t *client_block, uint16_t foreign_mid, uint32_t protocol, uint16_t association)
{
  syslog(LOG_DEBUG | LOG_LOCAL1,"sdt_format_client_block");

  /* skip flags and length for now */
  client_block += 2; 
  
  client_block += marshalU16(client_block, foreign_mid);     /* Vector */
  client_block += marshalU32(client_block, protocol);        /* Header, Client protocol */
  client_block += marshalU16(client_block, association);     /* Header, Association */
  
  /* returns pointer to opaque datagram */
  return client_block; 
}

/*****************************************************************************/
/*
  Marshal the channel parameter block
  paramBlock  - pointer to channel parameter block

  returns number of bytes marshaled.

  Currently these parameters are hard coded - if this changes this function will need more arguments
*/
static int
pack_channel_param_block(uint8_t *paramBlock)
{
  int offset = 0;
  
  offset += marshalU8(paramBlock + offset, FOREIGN_MEMBER_EXPIRY_TIME);
  offset += marshalU8(paramBlock + offset, 0); /* suppress OUTBOUND NAKs from foreign members */
  offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_HOLDOFF);
  offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_MODULUS);
  offset += marshalU16(paramBlock + offset, FOREIGN_MEMBER_NAK_MAX_WAIT);
  
  return offset;
}

/*****************************************************************************/
/*
  Unmarshal the channel parameter block
  member      - pointer to member to be populated
  paramBlock  - pointer to channel parameter block

  returns number of bytes marshaled.
*/
static int 
unpack_channel_param_block(sdt_member_t *member, uint8_t *param_block)
{
  member->expiry_time = *param_block;
  param_block++;
  
  member->nak = *param_block;
  param_block++;
  member->nak_holdoff = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  member->nak_modulus = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  member->nak_max_wait = unmarshalU16(param_block);
  param_block += sizeof(uint16_t);
  return 8; /* length of the ParamaterBlock */
}

/*****************************************************************************/
static int 
pack_transport_address(uint8_t *data, neti_addr_t *transport_addr, int type)
{
  uint8_t *datap;

  datap = data;

  switch(type) {
    case SDT_ADDR_NULL :
      datap += marshalU8(datap, SDT_ADDR_NULL);
      return datap - data;
      break;
    case SDT_ADDR_IPV4 :
      datap += marshalU8(datap, SDT_ADDR_IPV4);
      datap += marshalU16(datap, NETI_PORT(transport_addr));
      datap += marshalU32(datap, htonl(NETI_INADDR(transport_addr)));
      return datap - data;
      break;
    default :
      syslog(LOG_DEBUG | LOG_LOCAL1,"pack_transport_address: upsupported address type");
  }
  return 0;
}

/*****************************************************************************/
static int 
unpack_transport_address(uint8_t *data, neti_addr_t *transport_addr, neti_addr_t *packet_addr, uint8_t *type)
{
  uint8_t *datap;

  /* local pointer */
  datap = data;

  /* get and save type */
  *type = unmarshalU8(datap);
  datap += sizeof(uint8_t);
  
  switch(*type) {
    case SDT_ADDR_NULL :
      NETI_PORT(packet_addr) = NETI_PORT(transport_addr);
      NETI_INADDR(packet_addr) = NETI_INADDR(transport_addr);
      break;
    case SDT_ADDR_IPV4 :
      NETI_PORT(packet_addr) = unmarshalU16(datap);
      datap += sizeof(uint16_t);
      NETI_INADDR(packet_addr) = htonl(unmarshalU32(datap));
      datap += sizeof(uint32_t);
      break;
    case SDT_ADDR_IPV6 :
      syslog(LOG_DEBUG | LOG_LOCAL1,"unpack_transport_address: IPV6 not supported");
      break;
    default :
      syslog(LOG_DEBUG | LOG_LOCAL1,"unpack_transport_address: upsupported address type");
  }
  
  return datap - data;
}

#ifdef tick
/*****************************************************************************/
static void 
sdt_tick(void)
{
  sdt_leader_t *leader;
  sdt_leader_t **prevLeader;
  sdt_channel_t *channel;
  sdt_channel_t **prevChannel;
  
  sdt_member_t *member;
  sdt_member_t **prevMember;
  component_t  *remoteComp;

  sdt_wrapper_t wrapper; 
  
  leader = leaders;
  prevLeader = &leaders;
  
  while(leader) {
    prevChannel = &(leader->channelList);
    channel = leader->channelList;
    while(channel) {
      /* if the leader is local */
      if (channel->isLocal) {

//  /* Create packet buffer */
//  currentWrapper.txBuffer = rlpm_newtxbuf(DEFAULT_MTU, localComp->cid);
//  if (!currentWrapper.txBuffer) {
//    syslog(LOG_ERR | LOG_LOCAL1, "sdt_format_wrapper : failed to get new txbuf");
//    return 3;
//  } 

        /* send a MAK to all members in my downstream channel */
        sdt_format_wrapper(&wrapper, 0, leader->component, 1, 0xFFFF, 0);
        sdt_tx_current_wrapper();
      }

      prevMember = (sdt_member_t**) &(channel->memberList);
      member = (sdt_member_t*)channel->memberList;
      while(member) {
        if (timer_expired(member->expiresAt)) {
          if (member->isLocal) {
            /* send a leaving message */
            sdt_tx_leaving(leader, channel, member, SDT_LEAVE_CHANNEL_EXPIRED);
          } else {
            //command the member to leave my channel
            //sdtTxLeave(leader, channel, member);
            dmp_comp_offline_notify(member->component);
          }
          /* remove this member from the channel */
          *prevMember = member->next;
          ppFree(member, (member->isLocal) ? sizeof(sdt_member_t) : sizeof(sdt_member_t));
          member = *prevMember;
        } else {
          prevMember = &(member->next);
          member = member->next;
        }
      }
      if (!channel->memberList) { /* no members left in the channel */
        if (currentChannel == channel) {
          currentSrc = 0;
          currentChannel = 0;
          currentLeader =0;
        }
        mcast_leave(channel->downstreamIP);
        rlp_close_socket(channel->sock);
        /* remove the channel from the leader */
        *prevChannel = channel->next;
        ppFree(channel, sizeof(sdt_channel_t));
        channel = *prevChannel;
      } else {
        prevChannel = &(channel->next);
        channel = channel->next;
      }
    }
    if (!leader->channelList) { /* no channels left on this leader */
      remoteComp = leader->component;
      /* remove the leader */
      *prevLeader = leader->next;
      ppFree(leader, sizeof(sdt_leader_t));
      leader = *prevLeader;
      unregister_foreign_component(remoteComp->cid);
    } else {
      prevLeader = &(leader->next);
      leader = leader->next;
    }
  }
}
#endif

/*****************************************************************************/
int 
sdt_init(void)
{
  static bool initializedState = 0;

  if (initializedState) return 0;

  rlp_init();
  sdtm_init();
  initializedState = 1;
  return 0;
}

#if 0
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
  sdtm_add_component(comp, NULL, ??);
}
#endif

/*****************************************************************************/
void 
sdt_startup(bool acceptAdHoc)
{
  if (acceptAdHoc)
  {
    /* Open ad hoc channel on ephemerql port */
    if (!sdt_adhoc_socket) {
      sdt_adhoc_socket = rlp_open_netsocket(NETI_PORT_NONE);
    }
    /* now need to register with SLP passing port getLocalPort(adhoc) */
    //rlp_add_listener(sdt_adhoc_socket, NETI_GROUP_UNICAST, PROTO_SDT, sdt_rx_handler, NULL);
    //rlp_add_listener(struct netsocket_s *netsock, groupaddr_t groupaddr, protocolID_t protocol, rlpHandler_t *callback, void *ref)
  }
  // PN-FIXME must register SDT with RLP
  // PN-FIXME remove non-standard memory allocations
  /* FIXME register with SLP here */
//  addTask(5000, sdtTick, -1);  // PN-FIXME

}

/*****************************************************************************/
void 
sdt_shutdown(void)
{
  component_t     *component;
  sdt_channel_t   *channel;
  sdt_member_t    *member;
  
  /*  get first one! */
  component = sdtm_first_component();
  
  while(component) {
    channel = component->tx_channel;
    if (channel) {
      member = channel->member_list;
      while(member) {
        /* send a leaving message */
        //wrf sdt_tx_leaving(leader, channel, (sdt_member_t *)member, SDT_LEAVE_NONSPEC);
        member = sdtm_remove_member(channel, member);
      }
      /* shut down ports */
      if (channel->is_local) {
          /* disconnect the session */
          //wrf sdt_tx_disconnect(leader, channel, 0, PROTO_DMP);
          /* command all the members to leave my channel */
          //wrf sdt_tx_leave(leader, channel 0);
        if (channel->sock) {
          rlp_close_netsocket(channel->sock);
        }
      } else {
      }
      sdtm_remove_channel(component);
    }
    /* return the next component after delete */
    component = sdtm_remove_component(component);
  }
  rlp_close_netsocket(sdt_adhoc_socket);
  sdt_adhoc_socket = NULL;
}

#endif /* CONFIG_SDT */
