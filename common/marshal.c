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
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INUUIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	$Id$

*/
/*--------------------------------------------------------------------*/

#include <stddef.h>
#include <string.h>
#include "opt.h"
#include "acn_arch.h"
#include "marshal.h"

inline size_t marshalU8(uint8_t *data, uint8_t u8)
{
	*data = u8;
	return sizeof(uint8_t);
}

inline uint8_t unmarshalU8(const uint8_t *data)
{
	return *data;
}

inline size_t marshalU16(uint8_t *data, uint16_t u16)
{
	data[0] = (uint8_t)(u16 >> 8);
	data[1] = (uint8_t)u16;
	return sizeof(uint16_t);
}

inline uint16_t unmarshalU16(const uint8_t *data)
{
	return (uint16_t)((data[0] << 8) | data[1]);
}

inline size_t marshalU32(uint8_t *data, uint32_t u32)
{
	data[0] = (uint8_t)(u32 >> 24);
	data[1] = (uint8_t)(u32 >> 16);
	data[2] = (uint8_t)(u32 >> 8);
	data[3] = (uint8_t)u32;
	return sizeof(uint32_t);
}

inline uint32_t unmarshalU32(const uint8_t *data)
{
	return (uint32_t)(
			(uint32_t)(data[0] << 24)
			| (uint32_t)(data[1] << 16)
			| (uint32_t)(data[2] << 8)
			| (uint32_t)data[3]
		);
}

#if !defined(marshalUUID)
size_t marshalUUID(uint8_t *data, const uuid_t uuid)
{
	memcpy(data, uuid, sizeof(uuid_t));
  return sizeof(uuid_t);
}
#endif

#if !defined(unmarshalUUID)
uint8_t *unmarshalUUID(const uint8_t *data, uuid_t uuid)
{
	return memcpy(uuid, data, sizeof(uuid_t));
}
#endif

/*
  A variable length DMP value specifies the total length
  including the length field (min length is therefore 2)
  
  Internal p_strings have a min length of 0 and length specifies only
  the number of characters, not the total length
*/
inline size_t marshal_p_string(uint8_t *data, const p_string_t *str)
{
	marshalU16(data, str->length + 2);
	memcpy(data + sizeof(uint16_t), str->value, str->length);
	return str->length + sizeof(uint16_t);
}

inline size_t unmarshal_p_string(const uint8_t *data, p_string_t *str)
{
	str->length = unmarshalU16(data) - 2;
	memcpy(str->value, data + sizeof(uint16_t), str->length);
	return str->length + sizeof(uint16_t);
}
