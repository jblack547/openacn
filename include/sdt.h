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
/*
This header is the public face of SDT. It declares the API and definitions
necessary for accessing SDT from higher layers
*/

#ifndef __sdt_h__
#define __sdt_h__ 1

#include "opt.h"
#include "acn_arch.h"
#include "netiface.h"
#include "component.h"

/* ESTA registered protocol code */
#define SDT_PROTOCOL_ID     1
#define PROTO_SDT           SDT_PROTOCOL_ID

/* PDU vector codes [SDT spec Table 3] */
enum
{
  SDT_REL_WRAPPER     = 1,
  SDT_UNREL_WRAPPER   = 2,
  SDT_CHANNEL_PARAMS  = 3,
  SDT_JOIN            = 4,
  SDT_JOIN_REFUSE     = 5,
  SDT_JOIN_ACCEPT     = 6,
  SDT_LEAVE           = 7,
  SDT_LEAVING         = 8,
  SDT_CONNECT         = 9,
  SDT_CONNECT_ACCEPT  = 10,
  SDT_CONNECT_REFUSE  = 11,
  SDT_DISCONNECT      = 12,
  SDT_DISCONNECTING   = 13,
  SDT_ACK             = 14,
  SDT_NAK             = 15,
  SDT_GET_SESSIONS    = 16,
  SDT_SESSIONS        = 17, 
};

/* Reason codes [SDT spec Table 6] */
enum 
{
  SDT_REASON_NONSPEC          = 1,
  SDT_REASON_PARAMETERS       = 2,
  SDT_REASON_RESOURCES        = 3,
  SDT_REASON_ALREADY_MEMBER   = 4,
  SDT_REASON_BAD_ADDR         = 5,
  SDT_REASON_NO_RECIPROCAL    = 6,
  SDT_REASON_CHANNEL_EXPIRED  = 7,
  SDT_REASON_LOST_SEQUENCE    = 8,
  SDT_REASON_SATURATED        = 9,
  SDT_REASON_ADDR_CHANGING    = 10,
  SDT_REASON_ASKED_TO_LEAVE   = 11,
  SDT_REASON_NO_RECIPIENT     = 12,
  SDT_REASON_ONLY_UNICAST     = 13,
};

/* Address specification types [SDT spec Table 7] */
enum
{
  SDT_ADDR_NULL = 0,
  SDT_ADDR_IPV4 = 1,
  SDT_ADDR_IPV6 = 2,
};

#if 0   // MOVED TO component.h
typedef struct sdt_component_s
{
	struct sdt_component_s *next;
	cid_t         cid;
	cid_t         dcid;
  neti_addr_t   adhoc_addr;
	int           adhoc_expires_at;
  bool          is_local;
  struct sdt_channel_s *tx_channel;
} sdt_component_t;
#endif

typedef struct sdt_member_s
{
  struct sdt_member_s *next;
  component_t     *component;
  uint16_t mid;
  uint8_t  nak:1;
  uint8_t  pending:1;
  uint8_t  is_connected:1;
  uint8_t  expiry_time;
  uint32_t expires_at;
  
  /* only used for local member */
  uint16_t nak_holdoff;
  uint16_t last_acked;
  uint16_t nak_modulus;
  uint16_t nak_max_wait;
} sdt_member_t;

typedef struct sdt_channel_s
{
  uint16_t      number;
  uint16_t      available_mid;
  neti_addr_t   destination_addr;      // channel outbound address (multicast)
  neti_addr_t   channel_addr;         // channel source address
  bool          is_local;
  uint32_t      total_seq;
  uint32_t      reliable_seq;
  uint32_t      oldest_avail;
  sdt_member_t *member_list;
  struct netsocket_s *sock;           
} sdt_channel_t;

/* ok, just to make me not have to type stuct all the time */
typedef struct rlp_txbuf_s rlp_txbuf_t;

/* sequence errors */
enum
{
  SEQ_VALID,
  SEQ_NAK,
  SEQ_DUPLICATE,
  SEQ_OLD_PACKET,
  SEQ_LOST,
};

/* Wrapper type */
enum
{
  SDT_UNRELIABLE = false,
  SDT_RELIABLE = true
};

int      sdt_init(void);
void     sdt_startup(bool acceptAdHoc);
void     sdt_shutdown(void);
void     sdt_rx_handler(const uint8_t *data, int data_len, void *ref, const neti_addr_t *remhost, const cid_t foreign_cid);
//  def void rlpHandler_t(const uint8_t *data, int datasize, void *ref, const neti_addr_t *remhost, const cid_t remcid);


// **************************************************************
//TODO: these are only here for testing. Most will become static
// **************************************************************
/* BASE MESSAGES */
void     sdt_tx_join(component_t *local_component, component_t *foreign_component);
void     sdt_tx_join_accept(sdt_member_t *local_member, component_t *local_component, component_t *foreign_component);
void     sdt_tx_join_refuse(const cid_t foreign_cid, component_t *local_component, const neti_addr_t *transport_addr, 
           uint16_t foreign_channel_num, uint16_t local_mid, uint32_t foreign_rel_seq, uint8_t reason);
void     sdt_tx_leaving(component_t *foreign_component, component_t *local_component, uint8_t reason);
void     sdt_tx_nak(component_t *foreign_component, component_t *local_component, uint32_t last_missed);

//TODO:         sdt_tx_sessions()
//TODO:         sdt_tx_get_sessions()

void     sdt_rx_join(const cid_t foreign_cid, const neti_addr_t *transport_addr, const uint8_t *join, uint32_t data_len);
void     sdt_rx_join_accept(const cid_t foreign_cid, const uint8_t *join_accept, uint32_t data_len);
void     sdt_rx_join_refuse(const cid_t foreign_cid, const uint8_t *join_refuse, uint32_t data_len);
void     sdt_rx_leaving(const cid_t foreign_cid, const uint8_t *leaving, uint32_t data_len);
void     sdt_rx_nak(const cid_t foreign_cid, const uint8_t *nak, uint32_t data_len);
void     sdt_rx_wrapper(const cid_t foreign_cid, const neti_addr_t *transport_addr, const uint8_t *wrapper,  bool is_reliable, uint32_t data_len);
//TODO:         sdt_rx_get_sessions()
//TODO:         sdt_rx_sessions()

/* WRAPPED MESSAGES */
void     sdt_tx_ack(component_t *local_component, component_t *foreign_component);
//TODO:         sdt_tx_channel_params(sdt_wrapper_t *wrapper);
void     sdt_tx_leave(component_t *local_component, component_t *foreign_component);
void     sdt_tx_connect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
void     sdt_tx_connect_accept(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_connect_refuse(sdt_wrapper_t *wrapper);
void     sdt_tx_disconnect(component_t *local_component, component_t *foreign_component, uint32_t protocol);
//TODO:         sdt_tx_disconecting(sdt_wrapper_t *wrapper);

void     sdt_rx_ack(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_channel_params();
void     sdt_rx_leave(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
void     sdt_rx_connect(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
void     sdt_rx_connect_accept(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
//TODO:         sdt_rx_connect_refuse();
//TODO:         sdt_rx_disconnect();
//TODO:         sdt_rx_disconecting();

/* may go away, used for testing */
int  sdt_get_adhoc_port(void);



#endif
