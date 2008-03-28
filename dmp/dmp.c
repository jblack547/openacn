/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Pathway Connectivity Inc.

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

#include "opt.h"
#include "dmp.h"
#include "acn_dmp.h"
#include "sdt.h"
//#include "pdu.h"
#include <string.h>
//wrf #include <swap.h>
#include "acnlog.h"
//wrf #include <rtclock.h>
#include "marshal.h"
#include <datatypes.h>
//wrf #include  <ppmalloc.h>

/*
FIXME

Marshal functions have been updated and return a different type.
This source file has not.

*/

#if 0
typedef struct
{
  uint32_t startAddress;
  uint32_t addressInc;
  uint32_t numProps;
  uint32_t isSingleValue;
  uint32_t isVirtual;
} dmp_address_t;

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

static uint32_t lastActualAddress = 0;
static uint32_t lastVirtualAddress = 0;
static uint8_t propReplyBuf[1400];

// TODO: temps to get it to compile 
typedef struct 
{ 
  int   vector;
  int   vectorLength;
  int  *header;
  int   headerLength;
  char *data;
  int   dataLength;
  int   length;
} pdu_header_type;

#define WRITE_ONLY_ERROR 1
#define DATA_ERROR       2
#define READ_ONLY_ERROR  3
#define ALLOCATE_MAP_NOT_SUPPORTED 4
#define MAP_NOT_ALLOCATED 5
#define SUB_NOT_SUPPORTED 6

// END TEMPS

static inline void dmp_encode_address_header(pdu_header_type *pdu, dmp_address_t dmpAddress);
static inline int  dmp_decode_address_header(pdu_header_type *pdu, uint8_t *data, dmp_address_t *dmpAddress);

#endif

static uint32_t dmp_rx_get_property(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
static uint32_t dmp_rx_set_property(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len);
#if 0
              //dmp_rx_get_prop_reply 
              //dmp_rx_event
static uint32_t dmp_rx_map_property(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu);
              //dmp_rx_unmap_property
static uint32_t dmp_rx_subscribe(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu);
static uint32_t dmp_rx_unsubscribe(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu);
              //dmp_rx_get_property_fail
              //dmp_rx_set_property_fail
              //dmp_rx_map_property_fail
              //dmp_rx_subscribe_accept
              //dmp_subscribe_reject
static uint32_t dmp_rx_allocate_map(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu);
              //dmp_rx_allocate_map_reply
static uint32_t dmp_rx_deallocate_map(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu);

                //dmp_tx_get_property
                //dmp_tx_set_property
static void     dmp_tx_get_prop_reply(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t *reply, uint32_t replyLength);
                //dmp_tx_event
                //dmp_tx_map_property
                //dmp_tx_unmap_property
                //dmp_tx_subscribe
                //dmp_tx_unsubscribe
static void     dmp_tx_get_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode);
static void     dmp_tx_set_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode);
static void     dmp_tx_map_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode);
static void     dmp_tx_subscribe_accept(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address);
static void     dmp_tx_subscribe_reject(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode);
                //dmp_tx_allocate_map
static void     dmp_tx_allocate_map_reply(component_t *localComp, component_t *foreign_compoent, void *relatedSession, uint8_t reasonCode);
                //dmp_tx_deallocate_map

static void dmp_event_callback(component_t *localComp, uint32_t address, uint8_t *value, uint32_t valueLength);

/*****************************************************************************/
/* 
 */
void 
dmp_init(void)
{
  allocateToPool((void*)event_memory, sizeof(event_t) * MAX_QUEUED_EVENTS, sizeof(event_t));
  registerEventCallback(dmp_event_callback);
}
#endif


/*****************************************************************************/
/* Receive DMP the opaque datagram from an SDT ClientBlock
   This could contain 0 to N DMP messages
 */
void 
dmp_client_rx_handler(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
  const uint8_t    *data_end;
	const uint8_t    *pdup, *datap;
  uint8_t           vector   = 0;
	uint32_t          data_size = 0;
            
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

    /* we are going to assume there will be a reply sent back to the foreign component
       so we will go ahead and create a xmit buffer.  This way, all the messages can be put
       in one buffer
    */
    

    switch(vector) {
      case DMP_GET_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_GET_PROPERTY not supported..");
        dmp_rx_get_property(local_component, foreign_component, datap, data_size);
        break;
      case DMP_SET_PROPERTY:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SET_PROPERTY not supported..");
        dmp_rx_set_property(local_component, foreign_component, datap, data_size);
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
      case DMP_SUBSCRIBE:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_SUBSCRIBE not supported..");
        //dmp_rx_subscribe(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_UNSUBSCRIBE:
        acnlog(LOG_INFO | LOG_DMP,"dmp_client_rx_handler: DMP_UNSUBSCRIBE not supported..");
        //dmp_rx_unsubscribe(foreign_compoent, local_component, srcSession, &pdu);
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

#if 0
/*****************************************************************************/
/* Receive a client block
 */
uint32_t 
dmp_rx_handler(component_t *foreign_compoent, component_t *local_component, void *srcSession, uint8_t *data, uint32_t dataLen)
{
  uint32_t processed = 0;
  pdu_header_type pdu;
  
  //wrf benchmark.dmpStart = ticks;
  memset(&pdu, 0, sizeof(pdu_header_type));
  lastActualAddress = 0;
  lastVirtualAddress = 0;
  
  
  while(processed < dataLen) {
    //decode the pdu header
    processed += decodePdu(data + processed, &pdu, DMP_VECTOR_LEN, DMP_HEADER_LEN);

    ;//(LOG_DEBUG|LOG_LOCAL1,"dmpRxHandler: root pdu header decoded");
    
    //dispatch to vector
    switch(pdu.vector) {
      case DMP_SET_PROPERTY :
        set_active = 1;
        //dmp_rx_set_property(foreign_compoent, local_component, srcSession, &pdu);
        set_active = 0;
        break;
      case DMP_GET_PROPERTY :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_get_prop");
        //dmp_get_prop(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_ALLOCATE_MAP :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_rx_allocate_map");
        //dmp_rx_allocate_map(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_DEALLOCATE_MAP :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_rx_deallocate_map");
        //dmp_rx_deallocate_map(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_MAP_PROPERTY :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_rx_map_property");
        //dmp_rx_map_property(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_UNMAP_PROPERTY :
        ;//acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpUnmapProp");
        break;
      case DMP_SUBSCRIBE :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_rx_subscribe");
        //dmp_rx_subscribe(foreign_compoent, local_component, srcSession, &pdu);
        break;
      case DMP_UNSUBSCRIBE :
        //acnlog(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmp_rx_unsubscribe");
        //dmp_rx_unsubscribe(foreign_compoent, local_component, srcSession, &pdu);
        break;
      default:
        ;//acnlog(LOG_WARNING|LOG_LOCAL4,"dmpRxHandler: Unknown Vector (protocol) - skipping");
        
    }
//    acnlog(LOG_DEBUG |LOG_LOCAL4,"dmpRxHandler: Processed V:%d, L:%d", pdu.vector, processed);
  }
  if(event_queue)
    sdtFlush();
  while(event_queue) {
    event_t *event = event_queue;
    dmp_event_callback(event->comp, event->address, event->value, event->length);
    event_queue = event->next;
    ppFree(event, sizeof(event_t));
  }
  if(processed != dataLen) {
    acnlog(LOG_ERR|LOG_LOCAL4,"dmpRxHandler: processed an incorrect number of bytes");
  }
  
//  acnlog(LOG_DEBUG |LOG_LOCAL4,"dmpRxHandler:END");
  //wrf benchmark.dmpEnd = ticks;
  return dataLen;
}

//this DMP implementation is currently limited to sending single items ranges of equal sized data arrays
//All address generated by this function are absolute (non relative, non virtual)
// with the encoding of metadata in the DMP addresses all of the addresses are 4 octets long
/*****************************************************************************/
/* 
 */
static inline void 
dmp_encode_address_header(pdu_header_type *pdu, dmp_address_t dmpAddress)
{
  int length = 0;
  
  if(dmpAddress.numProps == 1)
    dmpAddress.isSingleValue = 1;
  
  *(pdu->header)  = FOUR_OCTET_ADDRESS;
  *(pdu->header) |= (dmpAddress.numProps > 1) ? RANGE_ADDRESS_EQUAL_SIZE_DATA : 0;

  length += marshalU32(pdu->data , dmpAddress.startAddress);
  if(dmpAddress.numProps > 1) {
    length += marshalU32(pdu->data + length, dmpAddress.addressInc);
    length += marshalU32(pdu->data + length, dmpAddress.numProps);
  }  
  pdu->dataLength += length;
}

/*****************************************************************************/
/* 
 */
static inline int 
dmp_decode_address_header(pdu_header_type *pdu, uint8_t *data, dmp_address_t *dmpAddress)
{
  //this could be made more efficient by caching the header and detecting an inherited header
  uint8_t *top = data;
  int size = 0;
  
  switch((*(pdu->header)) & ADDRESS_SIZE_MASK) { //Could be done as 2 ^ (pdu->header & ADDRESS_SIZE_MASK)
    case ONE_OCTET_ADDRESS :
      size = 1;
      break;
    case TWO_OCTET_ADDRESS :
      size = 2;
      break;
    case FOUR_OCTET_ADDRESS :
      size = 4;
      break;
    default :
      acnlog(LOG_LOCAL4 | LOG_ERR, "ERROR Controller sent RESERVED address Length");
  }
  
  switch(size)  {
    case 1 :
      dmpAddress->startAddress = unmarshalU8(data);
      break;
    case 2 :
      dmpAddress->startAddress = unmarshalU16(data);
      break;
    case 4 :
      dmpAddress->startAddress = unmarshalU32(data);
      break;
  }
  data += size;
  
  if(*pdu->header & ADDRESS_TYPE_MASK) {
    switch(size) {
      case 1 :
        dmpAddress->addressInc = unmarshalU8(data);
        break;
      case 2 :
        dmpAddress->addressInc = unmarshalU16(data);
        break;
      case 4 :
        dmpAddress->addressInc = unmarshalU32(data);
        break;
    }
    data += size;
    switch(size) {
      case 1 :
        dmpAddress->numProps = unmarshalU8(data);
        break;
      case 2 :
        dmpAddress->numProps = unmarshalU16(data);
        break;
      case 4 :
        dmpAddress->numProps = unmarshalU32(data);
        break;
    }
    data += size;
  } else {
    dmpAddress->numProps = 1;
    dmpAddress->addressInc = 0;
  }
  
  dmpAddress->isSingleValue = ((pdu->vector & ADDRESS_TYPE_MASK) < 0x20) ? 1 : 0;
  
  dmpAddress->isVirtual = (pdu->vector & VIRTUAL_ADDRESS_BIT);
  
  if(pdu->vector & RELATIVE_ADDRESS_BIT) {
    dmpAddress->startAddress += (dmpAddress->isVirtual) ? lastVirtualAddress : lastActualAddress;
    if(dmpAddress->isVirtual)
      lastVirtualAddress += dmpAddress->numProps * dmpAddress->addressInc;
    else
      lastActualAddress += dmpAddress->numProps * dmpAddress->addressInc;
  } else {
    if(dmpAddress->isVirtual)
      lastVirtualAddress += dmpAddress->startAddress;
    else
      lastActualAddress += dmpAddress->startAddress;
  }
  return data - top;
}
#endif

/*****************************************************************************/
/* 
 */
static uint32_t 
dmp_rx_get_property(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
#if 0
  dmp_address_t dmpAddress;
  dmp_address_t replyAddress;
  
  uint32_t processed = 0;
  int result;
  uint8_t *curPos;
  int lastResult;

  curPos = propReplyBuf;
  
  while(processed < pdu->dataLength)  {
    processed += dmp_decode_address_header(pdu, pdu->data + processed, &dmpAddress);

    if(dmpAddress.isVirtual) {
      // until we accept mapping we're done here
      return pdu->dataLength + processed;
    }
  
    replyAddress.isVirtual = 0;
    replyAddress.isSingleValue = 0;
    replyAddress.startAddress = dmpAddress.startAddress;
    replyAddress.addressInc = dmpAddress.addressInc;
    replyAddress.numProps = 0;

    while(dmpAddress.numProps) {
      result = (dmpAddress.startAddress & PROP_READ_BIT) ? getProperty(local_component, dmpAddress.startAddress, curPos) : WRITE_ONLY_ERROR;

      dmpAddress.numProps--;
      dmpAddress.startAddress += dmpAddress.addressInc;
      curPos += (result < 0) ? 0 : result;
      if((result != lastResult) && replyAddress.numProps) {
        if(lastResult < 0) {
//          acnlog(LOG_DEBUG|LOG_LOCAL4,"dmp_rx_get_property: fail within loop");
          dmp_tx_get_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
        } else {
//          acnlog(LOG_DEBUG|LOG_LOCAL4,"dmp_rx_get_property: success within loop");
          dmp_tx_get_prop_reply(local_component, foreign_compoent, srcSession, replyAddress, propReplyBuf, curPos - propReplyBuf);
        }
        
        //reset
        curPos = propReplyBuf;
        replyAddress.startAddress = dmpAddress.startAddress;
        replyAddress.numProps = 0;
      }
      replyAddress.numProps++;
      lastResult = result;
    }
    if(lastResult < 0) {
//      acnlog(LOG_DEBUG|LOG_LOCAL4,"dmp_rx_get_property: fail at end");
      dmp_tx_get_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
    } else {
//      acnlog(LOG_DEBUG|LOG_LOCAL4,"dmp_rx_get_property: success at end");
      dmp_tx_get_prop_reply(local_component, foreign_compoent, srcSession, replyAddress, propReplyBuf, curPos - propReplyBuf);
    }
  }
  return pdu->dataLength + processed;  

#endif
  return 0;
}

/*      
      if(dmpAddress.startAddress & PROP_READ_BIT)
        result = getProperty(local_component, dmpAddress.startAddress, curPos);
      else
        result = WRITE_ONLY_ERROR;


      failed = (result < 0);
      
      if(failed != lastResult)
      {
        
      }
      
      
      
      if(result != lastResult)
      {
        //send off the results to this point
        if(lastResult < 0) //error condition
          dmp_tx_get_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
        else if(lastResult > 0) //sucess
          dmp_tx_get_prop_reply(local_component, foreign_compoent, srcSession, replyAddress, topOfBuffer, curPos - topOfBuffer);

        //begin a new pdu
        topOfBuffer = curPos;
        curPos += (result > 0) ? result : 0;
        replyAddress.startAddress = dmpAddress.startAddress;
        replyAddress.numProps = 1;
        lastResult = result;
      }
      else
      {
        replyAddress.numProps++;
        curPos += (result > 0) ? result : 0;
      }
      dmpAddress.startAddress += dmpAddress.addressInc;
      numProps--;
    }
  }
  if(result < 0)
    dmp_tx_get_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
  else
    dmp_tx_get_prop_reply(local_component, foreign_compoent, srcSession, replyAddress, topOfBuffer, curPos - topOfBuffer);
  return pdu->dataLength + processed;
}
*/

/*****************************************************************************/
/* 
 */
static uint32_t
dmp_rx_set_property(component_t *local_component, component_t *foreign_component, const uint8_t *data, uint32_t data_len)
{
#if 0
  dmp_address_t dmpAddress;
  dmp_address_t replyAddress;
  int isVariable = 0;
  int processed = 0;
  int result = 0;
  int lastResult = 0;
  int sizeofValue = 0;
  int marshaledSize = 0;
  
  while(processed < pdu->dataLength) {
    processed += dmp_decode_address_header(pdu, pdu->data + processed, &dmpAddress);
  
    if(dmpAddress.isVirtual) {
      // until we accept mapping we're done here
      return -1;
    }
  
    replyAddress.isVirtual = 0;
    replyAddress.isSingleValue = 0;
    replyAddress.addressInc = dmpAddress.addressInc;
    replyAddress.numProps = 0;
    //wrf benchmark.numberofdmpprops +=dmpAddress.numProps;
  
    while(dmpAddress.numProps)  {
      sizeofValue = dmpAddress.startAddress >> PROP_SIZE_SHIFT;
      if(!(dmpAddress.startAddress & PROP_FIXED_LEN_BIT)) { //varible length
        isVariable = 1;
        marshaledSize = unmarshalU16(pdu->data + processed);
        
        if((marshaledSize - 2) > sizeofValue) { //error sent value too long
          result = DATA_ERROR;// Data length fail condition
          sizeofValue = marshaledSize;
          goto failure;
        }
        sizeofValue = marshaledSize;
      }
      else
        isVariable = 0;
        
      if(dmpAddress.startAddress & PROP_WRITE_BIT)
          result = setProperty(local_component, dmpAddress.startAddress, pdu->data + processed, sizeofValue);
      else
        result = READ_ONLY_ERROR;//fail read only property
      
failure:
      if(result != lastResult) {
        //send off the results to this point
        if(lastResult < 0) //error condition
          dmp_tx_set_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));

        //begin a new pdu
        replyAddress.startAddress = dmpAddress.startAddress;
        replyAddress.numProps = 1;
        lastResult = result;
      } else {
        replyAddress.numProps++;
      }
      if(!dmpAddress.isSingleValue)
        processed += sizeofValue;
      dmpAddress.startAddress += dmpAddress.addressInc;
      dmpAddress.numProps--;
    }
    if(result < 0) {
      dmp_tx_set_prop_fail(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
    }
    processed += sizeofValue;
  }
#endif
    
  return 1;
}

//dmp_rx_get_prop_reply 
//dmp_rx_event

#if 0

/*****************************************************************************/
/* 
 */
static uint32_t 
dmp_rx_map_property(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu)
{
  dmp_address_t dmpAddress;
  
  uint32_t processed = 0;
  
  processed += dmp_decode_address_header(pdu, pdu->data, &dmpAddress);
    
  dmp_tx_map_prop_fail(local_component, foreign_compoent, srcSession, dmpAddress, (MAP_NOT_ALLOCATED * -1));
  
  return processed;
}

//dmp_rx_unmap_property

/*****************************************************************************/
/* 
 */
static uint32_t 
dmp_rx_subscribe(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu)
{
  dmp_address_t dmpAddress;
  dmp_address_t replyAddress;
  
  uint32_t processed = 0;
  int result = 0;
  int previousResult = 1;
  
  processed += dmp_decode_address_header(pdu, pdu->data, &dmpAddress);
  
  if(dmpAddress.isVirtual) {
    // until we accept mapping we're done here
    return processed;
  }

  replyAddress.isVirtual = 0;
  replyAddress.isSingleValue = 0;
  replyAddress.addressInc = dmpAddress.addressInc;
  replyAddress.numProps = 0;
  replyAddress.startAddress = dmpAddress.startAddress;
//  acnlog(LOG_DEBUG |LOG_LOCAL4,"dmp_rx_subscribe : SA:0x%08x, AI:%d C:%d",
//    dmpAddress.startAddress, dmpAddress.addressInc, dmpAddress.numProps);
  while(dmpAddress.numProps) {
    if(dmpAddress.startAddress & PROP_EVENT_BIT)
      result = addSubscription(local_component, dmpAddress.startAddress, foreign_compoent, srcSession);
    else
      result = SUB_NOT_SUPPORTED;
//acnlog(LOG_DEBUG |LOG_LOCAL4,"dmp_rx_subscribe : A:0x%08x, R:%d", dmpAddress.startAddress, result);
          
    if(previousResult == 1) //unset      
      previousResult = result;
    if(result != previousResult) {
      if(previousResult < 0) //failure
        dmp_tx_subscribe_reject(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
      else //success
        dmp_tx_subscribe_accept(local_component, foreign_compoent, srcSession, replyAddress);

      replyAddress.startAddress = dmpAddress.startAddress;
      replyAddress.numProps = 1;
      previousResult = result;      
    } else
      replyAddress.numProps++;
    dmpAddress.startAddress += dmpAddress.addressInc;
    dmpAddress.numProps--;
  }
//  acnlog(LOG_DEBUG |LOG_LOCAL4,"dmp_rx_subscribe : Completed While");
  if(replyAddress.numProps) {
    if(previousResult < 0) //failure
      dmp_tx_subscribe_reject(local_component, foreign_compoent, srcSession, replyAddress, (result * -1));
    else //success
      dmp_tx_subscribe_accept(local_component, foreign_compoent, srcSession, replyAddress);
  }
  return processed;
}

/*****************************************************************************/
/* 
 */
static uint32_t 
dmp_rx_unsubscribe(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu)
{
  dmp_address_t dmpAddress;
  dmp_address_t replyAddress;
  
  uint32_t processed = 0;
  int result;
  int previousResult = 1;
  
  processed += dmp_decode_address_header(pdu, pdu->data, &dmpAddress);
    
  if(dmpAddress.isVirtual) {
    // until we accept mapping we're done here
    return processed;
  }

  replyAddress.isVirtual = 0;
  replyAddress.isSingleValue = 0;
  replyAddress.addressInc = dmpAddress.addressInc;
  replyAddress.numProps = 0;
  replyAddress.startAddress = dmpAddress.startAddress;
  
  while(dmpAddress.numProps) {
    result = removeSubscription(local_component, dmpAddress.startAddress, foreign_compoent, srcSession);
    if(previousResult == 1) //unset
      previousResult = result;
    if(result != previousResult)
    {
      if(previousResult < 0) //failure
        acnlog(LOG_ERR|LOG_LOCAL4,"dmp_rx_unsubscribe: Attempted to unsubscribe something not subscribed to");
      replyAddress.startAddress = dmpAddress.startAddress;
      replyAddress.numProps = 1;
      previousResult = result;      
    } else
      replyAddress.numProps++;
    dmpAddress.startAddress += dmpAddress.addressInc;
    dmpAddress.numProps--;
  }
  return processed;
}

//dmp_rx_get_property_fail
//dmp_rx_set_property_fail
//dmp_rx_map_property_fail
//dmp_rx_subscribe_accept
//dmp_subscribe_reject

/*****************************************************************************/
/* 
 */
static uint32_t
dmp_rx_allocate_map(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu)
{
  dmp_tx_allocate_map_reply(local_component, foreign_compoent, srcSession, (ALLOCATE_MAP_NOT_SUPPORTED * -1));
  return 0;
}

//dmp_rx_allocate_map_reply

/*****************************************************************************/
/* 
 */
static uint32_t 
dmp_rx_deallocate_map(component_t *foreign_compoent, component_t *local_component, void *srcSession, pdu_header_type *pdu)
{
  return 0;
}


//dmp_tx_get_property
//dmp_tx_set_property

/*****************************************************************************/
/* 
 */
static void 
dmp_tx_get_prop_reply(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t *reply, uint32_t replyLength)
{
  pdu_header_type replyHeader;
  uint8_t *buffer;
  
  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);
  
  replyHeader.vector = DMP_GET_PROPERTY_REPLY;
  replyHeader.vectorLength = 1;
  replyHeader.headerLength = 1;
  replyHeader.dataLength = 0;
  formatPdu(buffer, &replyHeader);
  
  
  dmp_encode_address_header(&replyHeader, address);
  memcpy(replyHeader.data + replyHeader.dataLength, reply, replyLength);
  replyHeader.dataLength += replyLength;
  
  updatePduLen(&replyHeader);
  enqueueClientBlock(replyHeader.length);
}

//dmp_tx_event
//dmp_tx_map_property
//dmp_tx_unmap_property
//dmp_tx_subscribe
//dmp_tx_unsubscribe


/*****************************************************************************/
/* 
 */
static void 
dmp_tx_get_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
  pdu_header_type replyHeader;
  uint8_t *buffer;
  int i;

  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);

  replyHeader.vector = DMP_GET_PROPERTY_FAIL;
  replyHeader.vectorLength = 1;
  replyHeader.headerLength = 1;
  replyHeader.dataLength = 0;  
  formatPdu(buffer, &replyHeader);
  
  
  dmp_encode_address_header(&replyHeader, address);
  for(i=0; i< address.numProps; i++) {
    replyHeader.dataLength += marshalU8(replyHeader.data + replyHeader.dataLength, reasonCode);
  }
  updatePduLen(&replyHeader);
  enqueueClientBlock(replyHeader.length);
}

/*****************************************************************************/
/* 
 */
static void 
dmp_tx_set_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
  pdu_header_type reply;
  uint8_t *buffer;
  int i;

  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);
  
  reply.vector = DMP_SET_PROPERTY_FAIL;
  reply.vectorLength = 1;
  reply.headerLength = 1;
  reply.dataLength = 0;  
  formatPdu(buffer, &reply);
  
  
  dmp_encode_address_header(&reply, address);
  for(i=0; i< address.numProps; i++) {
    reply.dataLength += marshalU8(reply.data + reply.dataLength, reasonCode);
  }
  updatePduLen(&reply);
  enqueueClientBlock(reply.length);
}

/*****************************************************************************/
/* 
 */
static void 
dmp_tx_map_prop_fail(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
  pdu_header_type reply;
  uint8_t header[6];
  
  reply.header = header;
  dmp_encode_address_header(&reply, address);
  reply.vector |= DMP_MAP_PROPERTY_FAIL;
  reply.data = relatedSession; //doesn't matter
  reply.dataLength = 0;
//  enqueueClientBlock(localComp, foreign_compoent, &reply);
}


/*****************************************************************************/
/* 
 */
static void 
dmp_tx_subscribe_accept(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address)
{
  uint8_t *buffer;
  pdu_header_type replyHeader;
    
  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);
  
  replyHeader.vector = DMP_SUBSCRIBE_ACCEPT;
  replyHeader.vectorLength = 1;
  replyHeader.headerLength = 1;
  replyHeader.dataLength = 0;
  formatPdu(buffer, &replyHeader);
  
  dmp_encode_address_header(&replyHeader, address);
  updatePduLen(&replyHeader);
  enqueueClientBlock(replyHeader.length);
//  acnlog(LOG_DEBUG |LOG_LOCAL4,"dmp_tx_subscribe_accept : Queued Accept");
}

/*****************************************************************************/
/* 
 */
static void 
dmp_tx_subscribe_reject(component_t *localComp, component_t *foreign_compoent, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
  int i;
  uint8_t *buffer;
  pdu_header_type replyHeader;
    
  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);
  
  replyHeader.vector = DMP_SUBSCRIBE_REJECT;
  replyHeader.vectorLength = 1;
  replyHeader.headerLength = 1;
  replyHeader.dataLength = 0;
  formatPdu(buffer, &replyHeader);
  
  dmp_encode_address_header(&replyHeader, address);
  for(i=0; i < address.numProps; i++)
    replyHeader.dataLength += marshalU8(replyHeader.data + replyHeader.dataLength, reasonCode);

  updatePduLen(&replyHeader);
  enqueueClientBlock(replyHeader.length);
}

//dmp_tx_allocate_map

/*****************************************************************************/
/* 
 */
static void 
dmp_tx_allocate_map_reply(component_t *localComp, component_t *foreign_compoent, void *relatedSession, uint8_t reasonCode)
{
  pdu_header_type reply;
  uint8_t *buffer;
  
  buffer = sdtFormatClientBlock(foreign_compoent, PROTO_DMP, relatedSession);
  
  reply.vector = DMP_ALLOCATE_MAP_REPLY;
  reply.vectorLength = 1;
  reply.headerLength = 1;
  reply.dataLength = 0;  
  formatPdu(buffer, &reply);
  
  reply.dataLength += marshalU8(reply.data, reasonCode);
  updatePduLen(&reply);
  enqueueClientBlock(reply.length);
}

//dmp_tx_deallocate_map



/*****************************************************************************/
/* 
 */
static void 
dmp_event_callback(component_t *localComp, uint32_t address, uint8_t *value, uint32_t valueLength)
{
  if(set_active) {
  //enqueue for later transmit
    event_t *event = (event_t*) ppMalloc(sizeof(event_t));
    if(!event)
      return;
    event->next = event_queue;
    event_queue = event;
    event->comp = localComp;
    event->address = address;
    event->value = value;
    event->length = valueLength;
    return;
  }
  pdu_header_type replyHeader;
  uint8_t *buffer;
  dmp_address_t dmpAddress;

  dmpAddress.startAddress = address;
  dmpAddress.numProps = 1;
  sdtFormatWrapper(isEventReliable(localComp, address), localComp, 0xFFFF, 0xFFFF, 0);
  buffer = sdtFormatClientBlock(0, PROTO_DMP, 0);
  
  replyHeader.vector = DMP_EVENT;
  replyHeader.vectorLength = 1;
  replyHeader.headerLength = 1;
  replyHeader.dataLength = 0;
  formatPdu(buffer, &replyHeader);
  dmp_encode_address_header(&replyHeader, dmpAddress);
  //acnlog(LOG_LOCAL4 | LOG_DEBUG, "dmp_event_callback");
  if(!(address & PROP_FIXED_LEN_BIT)) { //variable Length data
    replyHeader.dataLength += marshalU16(replyHeader.data + replyHeader.dataLength, valueLength + 2);
    memcpy(replyHeader.data + replyHeader.dataLength, value,  valueLength);
    replyHeader.dataLength += valueLength;    
  } else {
    switch(valueLength) {
      case 4 :
        replyHeader.dataLength += marshalU32(replyHeader.data + replyHeader.dataLength, *(uint32_t*)value);
        break;
      case 2 :
        replyHeader.dataLength += marshalU16(replyHeader.data + replyHeader.dataLength, *(uint16_t*)value);
        break;
      case 1 :
        replyHeader.dataLength += marshalU8(replyHeader.data + replyHeader.dataLength, *value);
        break;
    }
  }
  updatePduLen(&replyHeader);
  enqueueClientBlock(replyHeader.length);
  sdtFlush();
}

/*****************************************************************************/
/* 
 */
void 
dmpCompOfflineNotify(component_t *remoteComp)
{
  removeAllSubscriptions(remoteComp);
}
#endif
