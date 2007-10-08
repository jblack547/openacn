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
   
#include "pdu.h"
#include <string.h>
#include <swap.h>
#include <ctype.h>
#include <stdio.h>
#include <marshal.h>
#include <syslog.h>

int decodePdu(uint8 *buf, pdu_header_type *pdu, int vectorLen, int headerLen)
{
   int offset = 0;
	uint16 flags;

	flags = unmarshalU16(buf);
	pdu->flags = flags & (LENGTH_FLAG | VECTOR_FLAG | HEADER_FLAG | DATA_FLAG);
	
	if(pdu->flags & LENGTH_FLAG)
	{
		syslog(LOG_ERR | LOG_LOCAL0, "Error long length field not supported on this transport");
	}
	else
	{
	   pdu->length = (flags) & LENGTH_FIELD;
      offset += sizeof(uint16);
	}

	if(vectorPresent(flags))
	{
		switch(vectorLen)
		{
			case 1 :
				pdu->vector = unmarshalU8(buf + offset);
				break;
			case 2 :
				pdu->vector = unmarshalU16(buf + offset);
				break;
			case 3 : //huh?
				break;
			case 4 :
				pdu->vector = unmarshalU32(buf + offset);
				break;
		}
		offset += vectorLen;
	}
	
	if(headerPresent(flags))
	{
		pdu->header = buf + offset;
		pdu->headerLength = headerLen;
		offset += headerLen;
	}
	if(dataPresent(flags))
	{
		pdu->data = buf + offset;
		pdu->dataLength = pdu->length - offset;
	}
   return pdu->length;   
}

void updatePduLen(pdu_header_type *pdu)
{
	pdu->length = pdu->vectorLength + pdu->headerLength + pdu->dataLength + sizeof(uint16)/*flags*/;
	pdu->flags = VECTOR_FLAG | HEADER_FLAG | DATA_FLAG;
	marshalU16(pdu->location, (pdu->flags | pdu->length));
}

//Inheritance is not supported as yet.
//returns in pdu structure contains the header pointer and data pointers
//pdu structure vectorLen + headerLength must be set before calling

int formatPdu(uint8 *buf, pdu_header_type *pdu)
{
   int offset = sizeof(uint16);//marshalU16(buf, pdu->flags);
		
	pdu->location = buf;
	switch(pdu->vectorLength)
	{
		case 1 :
			offset += marshalU8(buf + offset, pdu->vector);
			break;
		case 2 :
			offset += marshalU16(buf + offset, pdu->vector);
			break;
		case 4 :
			offset += marshalU32(buf + offset, pdu->vector);
			break;
	}
	pdu->header = buf + offset;
	offset += pdu->headerLength;
	pdu->data = buf + offset;
	
   return offset;   
}


int calculatePduLength(pdu_header_type *header)
{
/*   header->length = sizeof(pdu_required_header_type);
   
   switch(header->dstAddrType)
   {
      case ACN_ADDRESS_16_BIT :
         header->length += sizeof(uint16);
	      break;
      case ACN_ADDRESS_128_BIT :
         header->length += sizeof(uuid_type);
         break;
      case ACN_ADDRESS_NOT_PRESENT :
      case ACN_ADDRESS_ALL :
         break;
   }
   switch(header->srcAddrType)
   {
      case ACN_ADDRESS_16_BIT :
         header->length += sizeof(uint16);
	      break;
      case ACN_ADDRESS_128_BIT :
         header->length += sizeof(uuid_type);
         break;
      case ACN_ADDRESS_NOT_PRESENT :
      case ACN_ADDRESS_ALL :
         break;
   }
   header->length += (header->protocol) ? sizeof(uint16) : 0;
   header->length += (header->type) ? sizeof(uint16) : 0;
   header->length += header->dataLength;
   header->length += (header->length > 255) ? sizeof(uint16) : 0; //optional extended length field
   header->length += (header->length > 0xFFFF) ? sizeof(uint16) : 0; //extend the optional length field
   
   return header->length;
  */
  return 0; 
}

