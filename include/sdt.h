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

/* sdt state */
typedef enum {
  ssCLOSED,
  ssSTARTED,
  ssCLOSING
} sdt_state_t;

/* channel buffer state */
//typedef enum
//{
//  bsOK,        /* closed pdu */
//  bsUNREL,    /* open unreliable wrapper */
//  bsREL       /* open reliable wrapper */
//} sdt_buf_state_t;

typedef enum
{
  msEMPTY     = 0,
  msPENDING   = 1,
  msJOINED    = 2,
  msCONNECTED = 3
} member_state_t;

/* buffer to use for sending */
//typedef struct sdt_txbuf_s
//{
//  struct rlp_txbuf_t    *rlp_buf;      /* buffer to send data on channel */
//  sdt_buf_state_t        buf_state;    
//  uint8_t               *blockstart;   /* start of PDU */
//  uint8_t               *blockend;     /* end of last PDU */
//  uint8_t               *data;         /* current pointer in databuffer */
//} sdt_txbuf_t;


typedef struct sdt_member_s
{
  struct sdt_member_s *next;
  component_t    *component;
  uint16_t        mid;
  uint8_t         nak:1;
  member_state_t  state;
  uint8_t         expiry_time_s;
  int             expires_ms;
  uint32_t        mak_ms;
  
  /* only used for local member */
  uint16_t nak_holdoff;
  //uint16_t last_acked;
  uint16_t nak_modulus;
  uint16_t nak_max_wait;
} sdt_member_t;

/* ok, just to make me not have to type stuct all the time */
typedef struct rlp_txbuf_s rlp_txbuf_t;

typedef struct sdt_channel_s
{
  uint16_t      number;
  uint16_t      available_mid;
  neti_addr_t   destination_addr;     // channel outbound address (multicast)
  neti_addr_t   source_addr;          // channel source address
  uint32_t      total_seq;
  uint32_t      reliable_seq;
  uint32_t      oldest_avail;
  sdt_member_t *member_list;
  struct netsocket_s    *sock; 
  struct rlp_listener_s *listener;     // multicast listener
//  sys_sem_t       tx_sem;                // todo: make generic
} sdt_channel_t;

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

#define NAK_OUTBOUND 0x80

int      sdt_init(void);
int      sdt_startup(bool acceptAdHoc);
int      sdt_shutdown(void);
void     sdt_rx_handler(const uint8_t *data, int data_len, void *ref, const neti_addr_t *remhost, const cid_t foreign_cid);
void     sdt_tick(void *arg);  /* timer call back */
int      sdt_join(component_t *local_component, component_t *foreign_component);

void     sdt_tx_reliable_data(component_t *local_component, component_t *foreign_component, uint32_t protocol, bool response, void *data, uint32_t data_len);
uint8_t *sdt_format_wrapper(uint8_t *wrapper, bool is_reliable, sdt_channel_t *local_channel, uint16_t first_mid, uint16_t last_mid, uint16_t mak_threshold);
uint8_t *sdt_format_client_block(uint8_t *client_block, uint16_t foreignlMember, uint32_t protocol, uint16_t association);


/* add with init */
component_t   *sdt_add_component(const cid_t cid, const cid_t dcid, bool is_local, access_t access);

sdt_channel_t *sdt_add_channel(component_t *leader, uint16_t channel_number);
sdt_member_t  *sdt_add_member(sdt_channel_t *channel, component_t *component);

sdt_member_t *sdt_find_member_by_mid(sdt_channel_t *channel, uint16_t mid);
sdt_member_t *sdt_find_member_by_component(sdt_channel_t *channel, component_t *component);

/* delete with cleanup */
component_t   *sdt_del_component(component_t *component);
sdt_channel_t *sdt_del_channel(component_t *leader);
sdt_member_t  *sdt_del_member(sdt_channel_t *channel, sdt_member_t *member);

/* misc */
component_t   *sdt_first_component(void);
component_t   *sdt_find_component(const cid_t cid);


//enum
//{
//  SDT_EVENT_JOIN_FAILED,
//  SDT_EVENT_JOIN_TERMINATED,
//  SDT_EVENT_JOINED,
//  SDT_EVENT_CONNECTED,
//  SDT_EVENT_DISCONNECTED
//};

/* may go away, used for testing */
int  sdt_get_adhoc_port(void);


#endif
