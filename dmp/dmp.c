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


/* **** NOTES ***************************
  This DMP interface has been striped of much of it original code. The current impementions
  pushes much of the DMP logic to the application layer where it can be process more
  efficiently.  This explains much of the code that has be commented out.
  
  It does this by parsing out the DMP messages and putting them in a queue which
  should be read by the application.

  while not fully impemented here yet, the intent is that besides filling the message queue to the
  application, this library will provide some routines that can be used by the application to help
  parse dmp message and to create and format DMP messages and replies.

  Bill Florac
  
  */
/*--------------------------------------------------------------------*/
static const char *rcsid __attribute__ ((unused)) =
   "$Id$";

#include "opt.h"
#include "dmp.h"
#include "acn_dmp.h"
#include "rlp.h"
#include "sdt.h"
#include "dmpmem.h"
#include "acnlog.h"


/*****************************************************************************/
/* 
 */
int
dmp_init(void)
{
  static bool initializedState = 0;

  /* Prevent reentry */
  if (initializedState) {
    acnlog(LOG_WARNING | LOG_DMP,"dmp_init: already initialized");
    return -1;
  }

  sdt_init();
  dmpm_init();
  
  initializedState = 1;
  return 0;
}

/*****************************************************************************/
/* Receive DMP the opaque datagram from an SDT ClientBlock
   This could contain 0 to N DMP messages
 */
void 
dmp_client_rx_handler(component_t *local_component, component_t *foreign_component, bool is_reliable, const uint8_t *data, uint32_t data_len, void *ref)
{
  const uint8_t    *data_end;
	const uint8_t    *pdup, *datap;
  uint8_t           vector   = 0;
	uint32_t          data_size = 0;
  sdt_member_t     *local_member;

//  LOG_FSTART();

  /* verify min data length */
  if (data_len < 3) {
		acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: Packet too short to be valid");
    return;
  }
  data_end = data + data_len;

  /* start of our pdu block containing one or multiple pdus*/
  pdup = data;

	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: illegal first PDU flags");
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
			acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: packet length error");
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
      case DMP_GET_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_SET_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SET_PROPERTY..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_GET_PROPERTY_REPLY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY_REPLY..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_EVENT:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_EVENT..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_MAP_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_MAP_PROPERTY not supported..");
        //dmp_rx_map_property(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_UNMAP_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_UNMAP_PROPERTY not supported..");
        //dmp_rx_unmap_property
        break;
      case DMP_SUBSCRIBE:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_UNSUBSCRIBE:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_UNSUBSCRIBE..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_GET_PROPERTY_FAIL:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY_FAIL..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_SET_PROPERTY_FAIL:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SET_PROPERTY_FAIL not supported..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_MAP_PROPERTY_FAIL:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_MAP_PROPERTY_FAIL not supported..");
        //
        break;
      case DMP_SUBSCRIBE_ACCEPT:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE_ACCEPT..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_SUBSCRIBE_REJECT:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE_REJECT..");
        dmpm_add_queue(local_component, foreign_component, is_reliable, datap, data_size, ref);
        break;
      case DMP_ALLOCATE_MAP:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_ALLOCATE_MAP not supported..");
        local_member = sdt_find_member_by_component(foreign_component->tx_channel, local_component);
        dmp_tx_allocate_map_reply(foreign_component, local_component, local_member, is_reliable, DMP_MAPS_NOT_SUPPORTED);
        break;
      case DMP_ALLOCATE_MAP_REPLY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_ALLOCATE_MAP_REPLY not supported..");
        //
        break;
      case DMP_DEALLOCATE_MAP:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_DEALLOCATE_MAP not supported..");
        //dmp_rx_deallocate_map(foreign_compoent, local_component, srcSession, &pdu);
        break;
    }
  }
//  LOG_FEND();
}


// FIRST OF MANY HELPER ROUTINES

/*****************************************************************************/
/*
  Send reply to ALLOCATE_MAP
    foreign_cid    - source of component sending NAK
    nak            - points to DATA section of NAK message
    data_len       - length of DATA section.
*/
void 
dmp_tx_allocate_map_reply(component_t *foreign_component, component_t *local_component, sdt_member_t *local_member, bool is_reliable, uint8_t reason)
{
  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *datagram;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;

//  LOG_FSTART();
  UNUSED_ARG(local_member);

  assert(local_component);
  assert(foreign_component);
  assert(foreign_component->tx_channel);

  foreign_channel = foreign_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_newtxbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_DMP, "dmp_tx_map_prop_fail : failed to get new txbuf");
    return;
  } 
  wrapper = rlp_init_block(tx_buffer, NULL);

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, is_reliable, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  datagram = sdt_format_client_block(client_block, foreign_member->mid, PROTO_DMP, foreign_channel->number);

  /* add datagram (ACK message) */
  datagram = marshalU16(datagram, 5 | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
  datagram = marshalU8(datagram, DMP_ALLOCATE_MAP_REPLY);
  datagram = marshalU8(datagram, 0);
  datagram = marshalU8(datagram, reason);

  /* add length to client block pdu */
  marshalU16(client_block, (5+7) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (datagram-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, datagram-wrapper, NULL);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_freetxbuf(tx_buffer);
}


