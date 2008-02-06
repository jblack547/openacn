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
// DMP uses LOG_LOCAL4
#include "opt.h"
#include "dmp.h"
#include "acn_dmp.h"
#include "sdt.h"
#include "pdu.h"
#include <string.h>
//wrf #include <swap.h>
#include <syslog.h>
//wrf #include <rtclock.h>
#include <marshal.h>
#include <datatypes.h>
//wrf #include	<ppmalloc.h>

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
	local_component_t *comp;
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

static inline void encodeAddressHeader(pdu_header_type *pdu, dmp_address_t dmpAddress);
static inline int decodeAddressHeader(pdu_header_type *pdu, uint8_t *data, dmp_address_t *dmpAddress);

static uint32_t dmpGetProp(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static void dmpTxGetPropReply(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t *reply, uint32_t replyLength);
static void dmpTxGetPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode);
static int dmpSetProp(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static void dmpTxSetPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode);

static int dmpRxAllocateMap(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static uint32_t dmpRxDeallocateMap(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static uint32_t dmpRxMapProperty(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static void dmpTxAllocateMapReply(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, uint8_t reasonCode);
static void dmpTxMapPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode);


static uint32_t dmpRxSubscribe(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static uint32_t dmpRxUnsubscribe(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu);
static void dmpTxSubscribeAccept(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address);
static void dmpTxSubscribeFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode);

static void dmpEventCallback(local_component_t *localComp, uint32_t address, uint8_t *value, uint32_t valueLength);

void initDmp(void)
{
	allocateToPool((void*)event_memory, sizeof(event_t) * MAX_QUEUED_EVENTS, sizeof(event_t));
	registerEventCallback(dmpEventCallback);
}

uint32_t dmpRxHandler(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, uint8_t *data, uint32_t dataLen)
{
	uint32_t processed = 0;
	pdu_header_type pdu;
	
	//wrf benchmark.dmpStart = ticks;
	memset(&pdu, 0, sizeof(pdu_header_type));
	lastActualAddress = 0;
	lastVirtualAddress = 0;
	
	
	while(processed < dataLen)
	{
		//decode the pdu header
		processed += decodePdu(data + processed, &pdu, DMP_VECTOR_LEN, DMP_HEADER_LEN);

		;//(LOG_DEBUG|LOG_LOCAL1,"dmpRxHandler: root pdu header decoded");
		
		//dispatch to vector
		switch(pdu.vector)
		{
			case DMP_SET_PROPERTY :
				set_active = 1;
				dmpSetProp(srcComp, dstComp, srcSession, &pdu);
				set_active = 0;
				break;
			case DMP_GET_PROPERTY :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpGetProp");
				dmpGetProp(srcComp, dstComp, srcSession, &pdu);
				break;
			case DMP_ALLOCATE_MAP :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpRxAllocateMap");
				dmpRxAllocateMap(srcComp, dstComp, srcSession, &pdu);
				break;
			case DMP_DEALLOCATE_MAP :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpRxDeallocateMap");
				dmpRxDeallocateMap(srcComp, dstComp, srcSession, &pdu);
				break;
			case DMP_MAP_PROPERTY :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpRxMapProperty");
				dmpRxMapProperty(srcComp, dstComp, srcSession, &pdu);
				break;
			case DMP_UNMAP_PROPERTY :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpUnmapProp");
				break;
			case DMP_SUBSCRIBE :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpRxSubscribe");
				dmpRxSubscribe(srcComp, dstComp, srcSession, &pdu);
				break;
			case DMP_UNSUBSCRIBE :
				;//(LOG_DEBUG|LOG_LOCAL4,"dmpRxHandler: Dispatch to dmpRxUnsubscribe");
				dmpRxUnsubscribe(srcComp, dstComp, srcSession, &pdu);
				break;
			default:
				;//(LOG_WARNING|LOG_LOCAL4,"dmpRxHandler: Unknown Vector (protocol) - skipping");
				
		}
//		syslog(LOG_DEBUG |LOG_LOCAL4,"dmpRxHandler: Processed V:%d, L:%d", pdu.vector, processed);
	}
	if(event_queue)
		sdtFlush();
	while(event_queue)
	{
		event_t *event = event_queue;
		dmpEventCallback(event->comp, event->address, event->value, event->length);
		event_queue = event->next;
		ppFree(event, sizeof(event_t));
	}
	if(processed != dataLen)
	{
		syslog(LOG_ERR|LOG_LOCAL4,"dmpRxHandler: processed an incorrect number of bytes");
	}
	
//	syslog(LOG_DEBUG |LOG_LOCAL4,"dmpRxHandler:END");
	//wrf benchmark.dmpEnd = ticks;
	return dataLen;
}

//this DMP implementation is currently limited to sending single items ranges of equal sized data arrays
//All address generated by this function are absolute (non relative, non virtual)
// with the encoding of metadata in the DMP addresses all of the addresses are 4 octets long
static inline void encodeAddressHeader(pdu_header_type *pdu, dmp_address_t dmpAddress)
{
	int length = 0;
	
	if(dmpAddress.numProps == 1)
		dmpAddress.isSingleValue = 1;
	
	*(pdu->header)	= FOUR_OCTET_ADDRESS;
	*(pdu->header) |= (dmpAddress.numProps > 1) ? RANGE_ADDRESS_EQUAL_SIZE_DATA : 0;

	length += marshalU32(pdu->data , dmpAddress.startAddress);
	if(dmpAddress.numProps > 1)
	{
		length += marshalU32(pdu->data + length, dmpAddress.addressInc);
		length += marshalU32(pdu->data + length, dmpAddress.numProps);
	}	
	pdu->dataLength += length;
}

static inline int decodeAddressHeader(pdu_header_type *pdu, uint8_t *data, dmp_address_t *dmpAddress)
{
	//this could be made more efficient by caching the header and detecting an inherited header
	uint8_t *top = data;
	int size = 0;
	
	switch((*(pdu->header)) & ADDRESS_SIZE_MASK)  //Could be done as 2 ^ (pdu->header & ADDRESS_SIZE_MASK)
	{
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
			syslog(LOG_LOCAL4 | LOG_ERR, "ERROR Controller sent RESERVED address Length");
	}
	
	switch(size)
	{
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
	
	if(*pdu->header & ADDRESS_TYPE_MASK)
	{
		switch(size)
		{
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
		switch(size)
		{
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
	}
	else
	{
		dmpAddress->numProps = 1;
		dmpAddress->addressInc = 0;
	}
	
	dmpAddress->isSingleValue = ((pdu->vector & ADDRESS_TYPE_MASK) < 0x20) ? 1 : 0;
	
	dmpAddress->isVirtual = (pdu->vector & VIRTUAL_ADDRESS_BIT);
	
	if(pdu->vector & RELATIVE_ADDRESS_BIT)
	{
		dmpAddress->startAddress += (dmpAddress->isVirtual) ? lastVirtualAddress : lastActualAddress;
		if(dmpAddress->isVirtual)
			lastVirtualAddress += dmpAddress->numProps * dmpAddress->addressInc;
		else
			lastActualAddress += dmpAddress->numProps * dmpAddress->addressInc;
	}
	else
	{
		if(dmpAddress->isVirtual)
			lastVirtualAddress += dmpAddress->startAddress;
		else
			lastActualAddress += dmpAddress->startAddress;
	}
	return data - top;
}


static uint32_t dmpGetProp(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmp_address_t dmpAddress;
	dmp_address_t replyAddress;
	
	uint32_t processed = 0;
	int result;
	uint8_t *curPos;
	int lastResult;

	curPos = propReplyBuf;
	
	while(processed < pdu->dataLength)
	{
		processed += decodeAddressHeader(pdu, pdu->data + processed, &dmpAddress);

		if(dmpAddress.isVirtual)
		{
			// until we accept mapping we're done here
			return pdu->dataLength + processed;
		}
	
		replyAddress.isVirtual = 0;
		replyAddress.isSingleValue = 0;
		replyAddress.startAddress = dmpAddress.startAddress;
		replyAddress.addressInc = dmpAddress.addressInc;
		replyAddress.numProps = 0;

		while(dmpAddress.numProps)
		{
			result = (dmpAddress.startAddress & PROP_READ_BIT) ? getProperty(dstComp, dmpAddress.startAddress, curPos) : WRITE_ONLY_ERROR;

			dmpAddress.numProps--;
			dmpAddress.startAddress += dmpAddress.addressInc;
			curPos += (result < 0) ? 0 : result;
			if((result != lastResult) && replyAddress.numProps)
			{
				if(lastResult < 0)
				{
//					syslog(LOG_DEBUG|LOG_LOCAL4,"dmpGetProp: fail within loop");
					dmpTxGetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
				}
				else
				{
//					syslog(LOG_DEBUG|LOG_LOCAL4,"dmpGetProp: success within loop");
					dmpTxGetPropReply(dstComp, srcComp, srcSession, replyAddress, propReplyBuf, curPos - propReplyBuf);
				}
				
				//reset
				curPos = propReplyBuf;
				replyAddress.startAddress = dmpAddress.startAddress;
				replyAddress.numProps = 0;
			}
			replyAddress.numProps++;
			lastResult = result;
		}
		if(lastResult < 0)
		{
//			syslog(LOG_DEBUG|LOG_LOCAL4,"dmpGetProp: fail at end");
			dmpTxGetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
		}
		else
		{
//			syslog(LOG_DEBUG|LOG_LOCAL4,"dmpGetProp: success at end");
			dmpTxGetPropReply(dstComp, srcComp, srcSession, replyAddress, propReplyBuf, curPos - propReplyBuf);
		}
	}
	return pdu->dataLength + processed;	
}

/*			
			if(dmpAddress.startAddress & PROP_READ_BIT)
				result = getProperty(dstComp, dmpAddress.startAddress, curPos);
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
					dmpTxGetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
				else if(lastResult > 0) //sucess
					dmpTxGetPropReply(dstComp, srcComp, srcSession, replyAddress, topOfBuffer, curPos - topOfBuffer);

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
		dmpTxGetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
	else
		dmpTxGetPropReply(dstComp, srcComp, srcSession, replyAddress, topOfBuffer, curPos - topOfBuffer);
	return pdu->dataLength + processed;
}
*/
static int dmpSetProp(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmp_address_t dmpAddress;
	dmp_address_t replyAddress;
	int isVariable = 0;
	int processed = 0;
	int result = 0;
	int lastResult = 0;
	int sizeofValue = 0;
	int marshaledSize = 0;
	
	while(processed < pdu->dataLength)
	{
		processed += decodeAddressHeader(pdu, pdu->data + processed, &dmpAddress);
	
		if(dmpAddress.isVirtual)
		{
			// until we accept mapping we're done here
			return -1;
		}
	
		replyAddress.isVirtual = 0;
		replyAddress.isSingleValue = 0;
		replyAddress.addressInc = dmpAddress.addressInc;
		replyAddress.numProps = 0;
		//wrf benchmark.numberofdmpprops +=dmpAddress.numProps;
	
		while(dmpAddress.numProps)
		{
			sizeofValue = dmpAddress.startAddress >> PROP_SIZE_SHIFT;
			if(!(dmpAddress.startAddress & PROP_FIXED_LEN_BIT)) //varible length
			{
				isVariable = 1;
				marshaledSize = unmarshalU16(pdu->data + processed);
				
				if((marshaledSize - 2) > sizeofValue) //error sent value too long
				{
					result = DATA_ERROR;// Data length fail condition
					sizeofValue = marshaledSize;
					goto failure;
				}
				sizeofValue = marshaledSize;
			}
			else
				isVariable = 0;
				
			if(dmpAddress.startAddress & PROP_WRITE_BIT)
					result = setProperty(dstComp, dmpAddress.startAddress, pdu->data + processed, sizeofValue);
			else
				result = READ_ONLY_ERROR;//fail read only property
			
failure:
			if(result != lastResult)
			{
				//send off the results to this point
				if(lastResult < 0) //error condition
					dmpTxSetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));

				//begin a new pdu
				replyAddress.startAddress = dmpAddress.startAddress;
				replyAddress.numProps = 1;
				lastResult = result;
			}
			else
			{
				replyAddress.numProps++;
			}
			if(!dmpAddress.isSingleValue)
				processed += sizeofValue;
			dmpAddress.startAddress += dmpAddress.addressInc;
			dmpAddress.numProps--;
		}
		if(result < 0)
		{
			dmpTxSetPropFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
		}
		processed += sizeofValue;
	}
		
	return 1;
}

static void dmpTxGetPropReply(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t *reply, uint32_t replyLength)
{
	pdu_header_type replyHeader;
	uint8_t *buffer;
	
	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);
	
	replyHeader.vector = DMP_GET_PROPERTY_REPLY;
	replyHeader.vectorLength = 1;
	replyHeader.headerLength = 1;
	replyHeader.dataLength = 0;
	formatPdu(buffer, &replyHeader);
	
	
	encodeAddressHeader(&replyHeader, address);
	memcpy(replyHeader.data + replyHeader.dataLength, reply, replyLength);
	replyHeader.dataLength += replyLength;
	
	updatePduLen(&replyHeader);
	enqueueClientBlock(replyHeader.length);
}



static void dmpTxGetPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
	pdu_header_type replyHeader;
	uint8_t *buffer;
	int i;

	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);

	replyHeader.vector = DMP_GET_PROPERTY_FAIL;
	replyHeader.vectorLength = 1;
	replyHeader.headerLength = 1;
	replyHeader.dataLength = 0;	
	formatPdu(buffer, &replyHeader);
	
	
	encodeAddressHeader(&replyHeader, address);
	for(i=0; i< address.numProps; i++)
	{
		replyHeader.dataLength += marshalU8(replyHeader.data + replyHeader.dataLength, reasonCode);
	}
	updatePduLen(&replyHeader);
	enqueueClientBlock(replyHeader.length);
}

static void dmpTxSetPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
	pdu_header_type reply;
	uint8_t *buffer;
	int i;

	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);
	
	reply.vector = DMP_SET_PROPERTY_FAIL;
	reply.vectorLength = 1;
	reply.headerLength = 1;
	reply.dataLength = 0;	
	formatPdu(buffer, &reply);
	
	
	encodeAddressHeader(&reply, address);
	for(i=0; i< address.numProps; i++)
	{
		reply.dataLength += marshalU8(reply.data + reply.dataLength, reasonCode);
	}
	updatePduLen(&reply);
	enqueueClientBlock(reply.length);
}

static int dmpRxAllocateMap(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmpTxAllocateMapReply(dstComp, srcComp, srcSession, (ALLOCATE_MAP_NOT_SUPPORTED * -1));
	return 0;
}

static uint32_t dmpRxDeallocateMap(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	return 0;
}

static void dmpTxAllocateMapReply(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, uint8_t reasonCode)
{
	pdu_header_type reply;
	uint8_t *buffer;
	
	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);
	
	reply.vector = DMP_ALLOCATE_MAP_REPLY;
	reply.vectorLength = 1;
	reply.headerLength = 1;
	reply.dataLength = 0;	
	formatPdu(buffer, &reply);
	
	reply.dataLength += marshalU8(reply.data, reasonCode);
	updatePduLen(&reply);
	enqueueClientBlock(reply.length);
}

static uint32_t dmpRxMapProperty(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmp_address_t dmpAddress;
	
	uint32_t processed = 0;
	
	processed += decodeAddressHeader(pdu, pdu->data, &dmpAddress);
		
	dmpTxMapPropFail(dstComp, srcComp, srcSession, dmpAddress, (MAP_NOT_ALLOCATED * -1));
	
	return processed;
}

static void dmpTxMapPropFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
	pdu_header_type reply;
	uint8_t header[6];
	
	reply.header = header;
	encodeAddressHeader(&reply, address);
	reply.vector |= DMP_MAP_PROPERTY_FAIL;
	reply.data = relatedSession; //doesn't matter
	reply.dataLength = 0;
//	enqueueClientBlock(localComp, foreignComp, &reply);
}

static uint32_t dmpRxSubscribe(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmp_address_t dmpAddress;
	dmp_address_t replyAddress;
	
	uint32_t processed = 0;
	int result = 0;
	int previousResult = 1;
	
	processed += decodeAddressHeader(pdu, pdu->data, &dmpAddress);
	
	if(dmpAddress.isVirtual)
	{
		// until we accept mapping we're done here
		return processed;
	}

	replyAddress.isVirtual = 0;
	replyAddress.isSingleValue = 0;
	replyAddress.addressInc = dmpAddress.addressInc;
	replyAddress.numProps = 0;
	replyAddress.startAddress = dmpAddress.startAddress;
//	syslog(LOG_DEBUG |LOG_LOCAL4,"dmpRxSubscribe : SA:0x%08x, AI:%d C:%d",
//		dmpAddress.startAddress, dmpAddress.addressInc, dmpAddress.numProps);
	while(dmpAddress.numProps)
	{
		if(dmpAddress.startAddress & PROP_EVENT_BIT)
			result = addSubscription(dstComp, dmpAddress.startAddress, srcComp, srcSession);
		else
			result = SUB_NOT_SUPPORTED;
//syslog(LOG_DEBUG |LOG_LOCAL4,"dmpRxSubscribe : A:0x%08x, R:%d", dmpAddress.startAddress, result);
					
		if(previousResult == 1) //unset			
			previousResult = result;
		if(result != previousResult)
		{
			if(previousResult < 0) //failure
				dmpTxSubscribeFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
			else //success
				dmpTxSubscribeAccept(dstComp, srcComp, srcSession, replyAddress);

			replyAddress.startAddress = dmpAddress.startAddress;
			replyAddress.numProps = 1;
			previousResult = result;			
		}
		else
			replyAddress.numProps++;
		dmpAddress.startAddress += dmpAddress.addressInc;
		dmpAddress.numProps--;
	}
//	syslog(LOG_DEBUG |LOG_LOCAL4,"dmpRxSubscribe : Completed While");
	if(replyAddress.numProps)
	{
		if(previousResult < 0) //failure
			dmpTxSubscribeFail(dstComp, srcComp, srcSession, replyAddress, (result * -1));
		else //success
			dmpTxSubscribeAccept(dstComp, srcComp, srcSession, replyAddress);
	}
	return processed;
}

static uint32_t dmpRxUnsubscribe(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, pdu_header_type *pdu)
{
	dmp_address_t dmpAddress;
	dmp_address_t replyAddress;
	
	uint32_t processed = 0;
	int result;
	int previousResult = 1;
	
	processed += decodeAddressHeader(pdu, pdu->data, &dmpAddress);
		
	if(dmpAddress.isVirtual)
	{
		// until we accept mapping we're done here
		return processed;
	}

	replyAddress.isVirtual = 0;
	replyAddress.isSingleValue = 0;
	replyAddress.addressInc = dmpAddress.addressInc;
	replyAddress.numProps = 0;
	replyAddress.startAddress = dmpAddress.startAddress;
	
	while(dmpAddress.numProps)
	{
		result = removeSubscription(dstComp, dmpAddress.startAddress, srcComp, srcSession);
		if(previousResult == 1) //unset
			previousResult = result;
		if(result != previousResult)
		{
			if(previousResult < 0) //failure
				syslog(LOG_ERR|LOG_LOCAL4,"dmpRxUnsubscribe: Attempted to unsubscribe something not subscribed to");
			replyAddress.startAddress = dmpAddress.startAddress;
			replyAddress.numProps = 1;
			previousResult = result;			
		}
		else
			replyAddress.numProps++;
		dmpAddress.startAddress += dmpAddress.addressInc;
		dmpAddress.numProps--;
	}
	return processed;
}

static void dmpTxSubscribeAccept(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address)
{
	uint8_t *buffer;
	pdu_header_type replyHeader;
		
	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);
	
	replyHeader.vector = DMP_SUBSCRIBE_ACCEPT;
	replyHeader.vectorLength = 1;
	replyHeader.headerLength = 1;
	replyHeader.dataLength = 0;
	formatPdu(buffer, &replyHeader);
	
	encodeAddressHeader(&replyHeader, address);
	updatePduLen(&replyHeader);
	enqueueClientBlock(replyHeader.length);
//	syslog(LOG_DEBUG |LOG_LOCAL4,"dmpTxSubscribeAccept : Queued Accept");
}

static void dmpTxSubscribeFail(local_component_t *localComp, foreign_component_t *foreignComp, void *relatedSession, dmp_address_t address, uint8_t reasonCode)
{
	int i;
	uint8_t *buffer;
	pdu_header_type replyHeader;
		
	buffer = sdtFormatClientBlock(foreignComp, PROTO_DMP, relatedSession);
	
	replyHeader.vector = DMP_SUBSCRIBE_REJECT;
	replyHeader.vectorLength = 1;
	replyHeader.headerLength = 1;
	replyHeader.dataLength = 0;
	formatPdu(buffer, &replyHeader);
	
	encodeAddressHeader(&replyHeader, address);
	for(i=0; i < address.numProps; i++)
		replyHeader.dataLength += marshalU8(replyHeader.data + replyHeader.dataLength, reasonCode);

	updatePduLen(&replyHeader);
	enqueueClientBlock(replyHeader.length);
}

static void dmpEventCallback(local_component_t *localComp, uint32_t address, uint8_t *value, uint32_t valueLength)
{
	if(set_active)
	{
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
	encodeAddressHeader(&replyHeader, dmpAddress);
	//syslog(LOG_LOCAL4 | LOG_DEBUG, "dmpEventCallback");
	if(!(address & PROP_FIXED_LEN_BIT)) //variable Length data
	{
		replyHeader.dataLength += marshalU16(replyHeader.data + replyHeader.dataLength, valueLength + 2);
		memcpy(replyHeader.data + replyHeader.dataLength, value,  valueLength);
		replyHeader.dataLength += valueLength;		
	}
	else
	{
		switch(valueLength)
		{
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

void dmpCompOfflineNotify(foreign_component_t *remoteComp)
{
	removeAllSubscriptions(remoteComp);
}
