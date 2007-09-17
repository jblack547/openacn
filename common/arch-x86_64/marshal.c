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

#include <marshal.h>

inline uint32 marshalU8(uint8 *data, uint8 u8)
{
	*data = u8;
	return sizeof(uint8);
}

inline uint8 unmarshalU8(uint8 *data)
{
	return *data;
}

inline uint32 marshalU16(uint8 *data, uint16 u16)
{
	uint16 temp;
	
	temp = htons(u16);
	memcpy(data, &temp, sizeof(uint16));
	return sizeof(uint16);
}

inline uint16 unmarshalU16(uint8 *data)
{
	uint16 temp;
	
	memcpy(&temp, data, sizeof(uint16));

	return ntohs(temp);
}

inline uint32 marshalU32(uint8 *data, uint32 u32)
{
	uint32 temp;
	
	temp = htonl(u32);
	memcpy(data, &temp, sizeof(uint32));
	return sizeof(uint32);
}

inline uint32 unmarshalU32(uint8 *data)
{
	uint32 temp;
	
	memcpy(&temp, data, sizeof(uint32));

	return ntohl(temp);
}

inline int marshalCID(uint8 *data, uint8 *cid)
{
	memcpy(data, cid, sizeof(uuid_type));
	return sizeof(uuid_type);
}

inline int unmarshalCID(uint8 *data, uint8 *cid)
{
	memcpy(cid, data, sizeof(uuid_type));
	return sizeof(uuid_type);
}

inline int marshal_p_string(uint8 *data, p_string_t *str)
{
	marshalU16(data, str->length + 2);
	memcpy(data + sizeof(uint16), str->value, str->length);
	return str->length + sizeof(uint16);
}

inline int unmarshal_p_string(uint8 *data, p_string_t *str)
{
	str->length = unmarshalU16(data) - 2;
	memcpy(str->value, data + sizeof(uint16), str->length);
	return str->length + sizeof(uint16);
}

