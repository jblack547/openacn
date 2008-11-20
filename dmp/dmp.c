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
//static const char *rcsid __attribute__ ((unused)) =
//   "$Id$";

/* System includes */
#include <string.h>
#include <datatypes.h>
#include <assert.h>

/* OpenACN includes */
#include "opt.h"
#include "types.h"
#include "acn_arch.h"

#if CONFIG_DMP
#include "dmp.h"
#include "dmpmem.h"
#include "acn_dmp.h"
#include "rlp.h"
#include "sdt.h"
#include "acnlog.h"
#include "marshal.h"

// TODO: remove this dependency. Add register and callback from application?
#if CONFIG_STACK_NETBURNER
#include "../../AcnDmp.h"
#endif

uint32_t lastActualAddress = 0;
uint32_t lastVirtualAddress = 0;

#if 0
typedef struct event_t
{
  struct event_t *next;
  component_t *comp;
  int address;
  uint8_t* value;
  int length;
} event_t;

#define MAX_QUEUED_EVENTS 24

static event_t *event_queue = 0;
static int set_active = 0;
static event_t event_memory[MAX_QUEUED_EVENTS];

static uint8_t propReplyBuf[1400];

// TODO: temps to get it to compile

#define WRITE_ONLY_ERROR 1
#define DATA_ERROR       2
#define READ_ONLY_ERROR  3
#define ALLOCATE_MAP_NOT_SUPPORTED 4
#define MAP_NOT_ALLOCATED 5
#define SUB_NOT_SUPPORTED 6

#endif  // if 0


/*****************************************************************************/
/* Receive the DMP opaque datagram from an SDT ClientBlock
   This could contain 0 to n DMP messages
 */
void
dmp_client_rx_handler(component_t *local_component, component_t *foreign_component, bool is_reliable, const uint8_t *data, uint32_t data_len, void *ref)
{
  uint8_t   *datap = 0;
  uint8_t   *pdup;
  uint8_t   *data_end;
  uint32_t   data_size = 0;
  uint8_t    vector = 0;
  uint8_t    header = 0;

  UNUSED_ARG(is_reliable); // this could be used in the future
  UNUSED_ARG(ref);

  // data points to the first byte (flags byte) of a specific DMP PDU message (like Set Property)
  // data_len is the total number of bytes in th DMP PDU

  // LOG_FSTART();

  /* verify min data length */
  if (data_len < 3) {
	  acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: Packet too short to be valid");
    return;
  }

  // find end of data
  data_end = (uint8_t*)(data) + data_len; // points to first byte past this PDU

  // start of our pdu block containing one or multiple pdus
  pdup = (uint8_t*)data; // points to flags byte

  // check for valid flags in the first pdu (there may be only one pdu)
  if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: illegal first PDU flags");
		return;
  }

  // while there is still a pdu to process
  while(pdup < data_end)  {
    uint8_t *pp; // pointer to curren PDU
	  uint8_t  flags; // flag byte

    /* get the flags from this pdu */
    flags = unmarshalU8(pdup);

    /* save pointer to this pdu */
    pp = pdup + 2; // points to vector (message type) field

    /* get pdu length and point to next pdu */
    pdup += getpdulen(pdup);

	// if length of this pdu is longer than we were given data for
	if (pdup > data_end) {
	  acnlog(LOG_WARNING | LOG_DMP,"dmp_client_rx_handler: packet length error");
	  return;
	}

    /* Get vector or inherit the last one if not present */
    /* vector is command (message type) */
	if (flags & VECTOR_bFLAG) {
      vector = unmarshalU8(pp);
  	  pp++;
    }

    /* Get header (Address/Data encode byte) or inherit the last one if not present */
    if (flags & HEADER_bFLAG) {
	  header = unmarshalU8(pp);   // header = address type
	  pp++;
	}

    /* Get packet data or inherit pointer to the last one if not present */
    if (flags & DATA_bFLAG) {
      datap = pp;
      /* calculate data length */
      data_size = pdup - pp;
    }

	switch(vector) {
	  case DMP_GET_PROPERTY:
	    acnlog(LOG_DEBUG | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY");
	    //app_rx_get_property(local_component, foreign_component, header, datap, data_size);
	    break;
	  case DMP_SET_PROPERTY:
	    acnlog(LOG_DEBUG | LOG_DMP,"dmp_client_rx_handler: DMP_SET_PROPERTY");
	    //app_rx_subscribe(local_component, foreign_component, header, datap, data_size); // DEBUG
	    //app_rx_set_property(local_component, foreign_component, header, datap, data_size);
	    break;
	  case DMP_SUBSCRIBE:
	    acnlog(LOG_DEBUG | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE");
	    //app_rx_subscribe(local_component, foreign_component, header, datap, data_size);
	    break;
	  case DMP_UNSUBSCRIBE:
	    acnlog(LOG_DEBUG | LOG_DMP,"dmp_client_rx_handler: DMP_UNSUBSCRIBE");
	    //app_rx_unsubscribe(local_component, foreign_component, header, datap, data_size);
	    break;
	  case DMP_GET_PROPERTY_REPLY:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY_REPLY not supported..");
	    //dmp_rx_get_prop_reply
	    break;
	  case DMP_EVENT:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_EVENT not supported..");
	    //dmp_rx_event
	    break;
	  case DMP_MAP_PROPERTY:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_MAP_PROPERTY not supported..");
	    //dmp_rx_map_property(foreign_compoent, local_component, srcSession, &pdu);
	    break;
	  case DMP_UNMAP_PROPERTY:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_UNMAP_PROPERTY not supported..");
	    //dmp_rx_unmap_property
	    break;
	  case DMP_GET_PROPERTY_FAIL:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY_FAIL not supported..");
	    //dmp_rx_get_property_fail
	    break;
	  case DMP_SET_PROPERTY_FAIL:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SET_PROPERTY_FAIL not supported..");
	    //dmp_rx_set_property_fail
	    break;
	  case DMP_MAP_PROPERTY_FAIL:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_MAP_PROPERTY_FAIL not supported..");
	    //dmp_rx_map_property_fail
	    break;
	  case DMP_SUBSCRIBE_ACCEPT:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE_ACCEPT not supported..");
	    //dmp_rx_subscribe_accept
	    break;
	  case DMP_SUBSCRIBE_REJECT:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE_REJECT not supported..");
	    //dmp_subscribe_reject
	    break;
	  case DMP_ALLOCATE_MAP:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_ALLOCATE_MAP not supported..");
	    //dmp_rx_allocate_map(foreign_compoent, local_component, srcSession, &pdu);
	    break;
	  case DMP_ALLOCATE_MAP_REPLY:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_ALLOCATE_MAP_REPLY not supported..");
	    //dmp_rx_allocate_map_reply
	    break;
	  case DMP_DEALLOCATE_MAP:
	    acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_DEALLOCATE_MAP not supported..");
	    //dmp_rx_deallocate_map(foreign_compoent, local_component, srcSession, &pdu);
	    break;
    }
  }
//  LOG_FEND();
}

/*****************************************************************************/
// All addresses generated by this function are absolute (non relative, non virtual).
// Inputs:
//  dmp_address  struct initialized with address info to be transated into the PDU
//  encode_byte*  pointer to where the Address/Data Encode byte will be written
//  data*        pointer to where the address will be written. For first address of
//               the PDU this will be the byte following the encode_byte.
/*****************************************************************************/
uint8_t* dmp_encode_address_header(dmp_address_t *dmp_address, uint8_t *encode_byte, uint8_t *datap)
{
  // TODO: Relative addressing bit..?

  // write the Address/Data encoded byte
  *encode_byte = (uint8_t)(dmp_address->address_size | dmp_address->address_type);
  if (dmp_address->is_virtual) {
  	*encode_byte |= VIRTUAL_ADDRESS_BIT;
  }

  // write the start address
  switch (dmp_address->address_size) {
    case ONE_OCTET_ADDRESS :
	  	datap = marshalU8(datap , (uint8_t)dmp_address->address_start);
    	break;
    case TWO_OCTET_ADDRESS :
	  	datap = marshalU16(datap , (uint8_t)dmp_address->address_start);
    	break;
    case FOUR_OCTET_ADDRESS :
	  	datap = marshalU32(datap , (uint8_t)dmp_address->address_start);
    	break;
    default:
      acnlog(LOG_WARNING | LOG_DMP,"dmp_encode_address_header: Address length not valid... %x", dmp_address->address_size);
  }

  // if the address type is non-zero then a range address was specified
  if (dmp_address->address_type) {
  	// write the address increment and number of properties
  	switch (dmp_address->address_size) {
  		case ONE_OCTET_ADDRESS:
		  	datap = marshalU8(datap , (uint8_t)dmp_address->address_inc);
		  	datap = marshalU8(datap , (uint8_t)dmp_address->num_props);
  			break;
  		case TWO_OCTET_ADDRESS:
		  	datap = marshalU16(datap , (uint16_t)dmp_address->address_inc);
		  	datap = marshalU16(datap , (uint16_t)dmp_address->num_props);
	  		break;
  		case FOUR_OCTET_ADDRESS:
		  	datap = marshalU32(datap , dmp_address->address_inc);
		  	datap = marshalU32(datap , dmp_address->num_props);
  			break;
  		default:
  		;
  	}
  }
  return datap;
}

//*******************************************************************************
// Decode the Address/Data encoded byte and use it to interpret the next address at data.
// Put the results into the dmp_address struct.
int dmp_decode_address_header(uint8_t address_type, uint8_t *data, dmp_address_t *dmp_address)
{

  uint8_t *start = data;
  int size = 0;

  // determine address size in bytes
  switch(address_type & ADDRESS_SIZE_MASK) {
    case ONE_OCTET_ADDRESS :
      //acnlog(LOG_DEBUG | LOG_DMP,"ONE_OCTET_ADDRESS");
      dmp_address->address_size = ONE_OCTET_ADDRESS;
      size = 1;
      break;
    case TWO_OCTET_ADDRESS :
      //acnlog(LOG_DEBUG | LOG_DMP,"TWO_OCTET_ADDRESS");
      dmp_address->address_size = TWO_OCTET_ADDRESS;
      size = 2;
      break;
    case FOUR_OCTET_ADDRESS :
      //acnlog(LOG_DEBUG | LOG_DMP,"FOUR_OCTET_ADDRESS");
      dmp_address->address_size = FOUR_OCTET_ADDRESS;
      size = 4;
      break;
    default : {
      acnlog(LOG_WARNING | LOG_DMP,"dmp_decode_address_header: Address length not valid..");
    }
  }

  // get the starting address
  switch(size)  {
    case 1 :
      dmp_address->address_start = unmarshalU8(data);
      break;
    case 2 :
      dmp_address->address_start = unmarshalU16(data);
      break;
    case 4 :
      dmp_address->address_start = unmarshalU32(data);
      break;
  }
  data += size;

  // get the address type
  dmp_address->address_type = address_type & ADDRESS_TYPE_MASK;

  // if address type is non-zero then a range address was specified
  if(dmp_address->address_type) {
  	// get the increment for the range
    switch(size) {
      case 1 :
        dmp_address->address_inc = unmarshalU8(data);
        break;
      case 2 :
        dmp_address->address_inc = unmarshalU16(data);
        break;
      case 4 :
        dmp_address->address_inc = unmarshalU32(data);
        break;
    }
    data += size;
    // get the count of properties
    switch(size) {
      case 1 :
        dmp_address->num_props = unmarshalU8(data);
        break;
      case 2 :
        dmp_address->num_props = unmarshalU16(data);
        break;
      case 4 :
        dmp_address->num_props = unmarshalU32(data);
        break;
    }
    data += size;
  } else {
  	// only one address
    dmp_address->num_props = 1;
    dmp_address->address_inc = 0;
  }

  // is there a single data item?
  dmp_address->is_single_value = ((address_type & ADDRESS_TYPE_MASK) < 0x20) ? 1 : 0;

  dmp_address->is_virtual = (address_type & VIRTUAL_ADDRESS_BIT);

  // if the address given is relative
  if (address_type & RELATIVE_ADDRESS_BIT) {
    // compute the address of the first property
    if(dmp_address->is_virtual) {
  	  dmp_address->address_start += lastVirtualAddress;
    } else {
  	  dmp_address->address_start += lastActualAddress;
    }
  }

  // compute the address of the last property and save for next time
  if(dmp_address->is_virtual) {
    lastVirtualAddress = dmp_address->address_start + (dmp_address->num_props-1) * dmp_address->address_inc;
  } else {
    lastActualAddress = dmp_address->address_start + (dmp_address->num_props-1) * dmp_address->address_inc;
  }

  return data - start; // number of bytes parsed
}

// create a data area to compose a message in
// TBD this should probably be changed to do a malloc() for multiple thread use
static uint8_t data[300];
//*******************************************************************************
// Create a Get Property Reply message and send it
void dmp_tx_get_prop_reply(component_t *local_component, component_t *foreign_component, dmp_address_t *address, uint8_t *ptrProperty, uint16_t sizeofProperty)
{
  uint8_t       *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;
  int           x;

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_GET_PROPERTY_REPLY);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // copy the property data
  for (x=0; x<sizeofProperty; x++) {
    *datap++ = *ptrProperty++;
  }

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

// TBD this routine needs to use a buffer other than "data" because it can interrupt the other tasks using it.
//*******************************************************************************
// Create an Event message and send it
void dmp_tx_event(component_t *local_component, component_t *foreign_component, dmp_address_t *address, uint8_t *ptrProperty, uint16_t sizeofProperty)
{
  uint8_t       *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;
  int           x;

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_EVENT);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // copy the property data
  for (x=0; x<sizeofProperty; x++) {
    *datap++ = *ptrProperty++;
  }

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

//*******************************************************************************
// Create a Send Property Fail message and send it
void dmp_tx_set_prop_fail(component_t *local_component, component_t *foreign_component, dmp_address_t *address, sint8_t result)
{
  uint8_t      *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_SET_PROPERTY_FAIL);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // put in the result field
  *datap++ = result;

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

//*******************************************************************************
// Create a Get Property Fail message and send it
void dmp_tx_get_prop_fail(component_t *local_component, component_t *foreign_component, dmp_address_t *address, sint8_t result)
{
  uint8_t       *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_GET_PROPERTY_FAIL);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // put in the result field
  *datap++ = result;

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

//*******************************************************************************
// Create a Subscribe Accept message and send it
void dmp_tx_subscribe_accept(component_t *local_component, component_t *foreign_component, dmp_address_t *address)
{
  uint8_t       *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;

  // This message must be sent on the connection (mine) to which events for the subcription are also sent.
  // TBD How is this done?

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_SUBSCRIBE_ACCEPT);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

//*******************************************************************************
// Create a Subscribe Reject message and send it
void dmp_tx_subscribe_reject(component_t *local_component, component_t *foreign_component, dmp_address_t *address, sint8_t result)
{
  uint8_t       *datap;  // temp pointer used for PDU
  int           data_len;  // length of this PDU
  bool          is_reliable = true;

  // get pointer into the PDU where the vector byte will be placed
  // we will come back later to set the first two bytes (flag and length)
  datap = data+2;

  // put the message type (vector) into the pdu
  datap = marshalU8(datap, DMP_SUBSCRIBE_REJECT);

  // encode address
  datap = dmp_encode_address_header(address, datap, datap+1);

  // put in the result field
  *datap++ = result;

  // find the length of this PDU
  data_len = datap - data;

  // go back and put the flags and length fields into the first two bytes
  marshalU16(data, VECTOR_FLAG | HEADER_FLAG | DATA_FLAG | data_len);

  // send this DMP PDU. The data will have to be copied by dmp_tx_pdu.
  dmp_tx_pdu(local_component, foreign_component, is_reliable, data, data_len);

}

/*****************************************************************************/
/*
  Send a DMP message
  The calling routine composes a PDU for a specific messsage and passes it in.
*/
void
dmp_tx_pdu(component_t *local_component, component_t *foreign_component, bool is_reliable, uint8_t *datap, int data_len)
{
  // datap is pointer to pdu (or pdu block) starting at the flags byte
  // data_len is length of pdu

  uint8_t       *wrapper;
  uint8_t       *client_block;
  uint8_t       *pdup;
  sdt_member_t  *foreign_member;
  rlp_txbuf_t   *tx_buffer;
  sdt_channel_t *foreign_channel;
  sdt_channel_t *local_channel;
  int           x;

  assert(local_component);
  assert(foreign_component);

  if (!foreign_component->tx_channel) {
    acnlog(LOG_WARNING | LOG_DMP, "dmp_tx_pdu : foreign component without channel");
    return;
  }

  assert(foreign_component->tx_channel);

  foreign_channel = foreign_component->tx_channel;
  local_channel = local_component->tx_channel;

  /* Create packet buffer */
  tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
  if (!tx_buffer) {
    acnlog(LOG_ERR | LOG_DMP, "dmp_tx_pdu : failed to get new txbuf");
    return;
  }
  wrapper = rlp_init_block(tx_buffer, NULL);

  foreign_member = sdt_find_member_by_component(local_channel, foreign_component);
  if (!foreign_member) {
    acnlog(LOG_WARNING | LOG_DMP, "dmp_tx_pdu : failed to find foreign member");
    return;
  }

  /* create a wrapper and get pointer to client block */
  client_block = sdt_format_wrapper(wrapper, is_reliable, local_channel, 0xffff, 0xffff, 0); /* no acks, 0 threshold */

  /* create client block and get pointer to opaque datagram */
  pdup = sdt_format_client_block(client_block, foreign_member->mid, PROTO_DMP, foreign_channel->number);

  // copy data to it starting with the flags byte
  for (x=0; x<data_len; x++) {
  	*pdup++ = *datap++;
  }

  /* add length to client block pdu */
  marshalU16(client_block, (data_len+10) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add length to wrapper pdu */
  marshalU16(wrapper, (pdup-wrapper) | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);

  /* add our PDU (sets its length there)*/
  rlp_add_pdu(tx_buffer, wrapper, pdup-wrapper, NULL);

  /* and send it on */
  rlp_send_block(tx_buffer, local_channel->sock, &local_channel->destination_addr);
  rlpm_release_txbuf(tx_buffer);

}

/*****************************************************************************/
/*
  find a subscription from the foreign component
*/
dmp_subscription_t *
dmp_find_subscription(component_t *foreign_component, dmp_property_t *property)
{
  dmp_subscription_t *subscription;

  subscription = foreign_component->subscriptions;
  // find our subscription
  while (subscription) {
    if (subscription->property == property) {
        return subscription;
    }
    subscription = subscription->next;
  }
  //acnlog(LOG_INFO | LOG_DMP,"dmp_find_subscription: subscription not found");
  return NULL;
}

/*****************************************************************************/
/*
  add a subscriptions
*/
dmp_subscription_t *
dmp_add_subscription(component_t *foreign_component, dmp_property_t *property)
{
  //TODO: add protect = ACN_PORT_PROTECT();

  dmp_subscription_t *subscription;

  subscription = dmp_find_subscription(foreign_component, property);

  // we can put there here in advance. if it get rejected, they will flush out after they expire
  if (subscription) {
    // already subscribed
    return subscription;
  }

  subscription = dmpm_new_subscription();
  if (subscription) {
      // increment our reference counter
      property->ref_count++;
      // assign our property
      subscription->property = property;
      //subscription.state = DMP_SUB_EMPTY;
      subscription->expires_ms = 1000;
      //subscription.next = NULL;

      // add this to our chain
      subscription->next = foreign_component->subscriptions;
      foreign_component->subscriptions = subscription;
      return subscription;
  }
  // oops, out of resources (logged by dmpm_new_subscription)
  //acnlog(LOG_WARNING | LOG_DMP,"dmp_add_subscription: out of subscriptions");
  return NULL;
}

/*****************************************************************************/
/*
  delete a subscriptions
*/
dmp_subscription_t *
dmp_del_subscription(component_t *foreign_component, dmp_property_t *property)
{
  dmp_subscription_t *subscription;
  dmp_subscription_t *cur_sub, *next_sub;
  //acn_protect_t  protect;

  // remove it from the chain
  /* if it is at top, then we just move leader */
  subscription = foreign_component->subscriptions;
  if (property->address == subscription->property->address) {
    //protect = ACN_PORT_PROTECT();
    // decrement our ref count
    subscription->property->ref_count--;
    // scoot around it
    foreign_component->subscriptions = subscription->next;
    // free memory
    dmpm_free_subscription(subscription);
    //ACN_PORT_UNPROTECT(protect);
    return foreign_component->subscriptions;
  }

  /* else run through list in order, finding previous one */
  cur_sub = foreign_component->subscriptions;
  while (cur_sub) {
    next_sub = cur_sub->next;
    // does it match
    if (property->address == next_sub->property->address) {
      /* yes, jump around it */
      //protect = ACN_PORT_PROTECT();
      // decrement our ref count
      property->ref_count--;
      // scoot around it
      cur_sub->next = next_sub->next;
      // free memory
      dmpm_free_subscription(next_sub);
      //ACN_PORT_UNPROTECT(protect);
      return cur_sub->next;
    }
    cur_sub = cur_sub->next;
  }
  acnlog(LOG_WARNING | LOG_DMP,"dmp_del_subscription: subscription not found");
  return NULL;
}


/*****************************************************************************/
/*
  do all the work to establish a subscription
*/
int
dmp_establish_subscription(component_t *local_component, component_t *foreign_component, dmp_property_t *property)
{
  sdt_member_t        *member = 0;
  dmp_subscription_t  *subscription = 0;

  // we can put there here in advance. if it get rejected, they will flush out after they expire

  // does it already exist
  subscription = dmp_find_subscription(foreign_component, property);
  if (subscription) {
    // already subscribed
    return 0; // ok
  }

  // add new one
  subscription = dmp_add_subscription(foreign_component, property);
  if (subscription) {
    if (local_component->tx_channel) {
      acnlog(LOG_INFO | LOG_DMP,"dmp_establish_subscription: using existing connection");
      member = sdt_find_member_by_component(local_component->tx_channel, foreign_component);
    }

    if (!member) {
      // sdt_tick will send out accept when connected
      acnlog(LOG_INFO | LOG_DMP,"dmp_establish_subscription: creating connection");
      sdt_join(local_component, foreign_component);
    }
  } else {
    // out of resources
    // logged by dmp_add_subscription...
    //acnlog(LOG_WARNING | LOG_DMP,"dmp_establish_subscription: out of subscriptoins");
    return -1;
  }
  return 0; //OK
}

#endif /* CONFIG_DMP */
