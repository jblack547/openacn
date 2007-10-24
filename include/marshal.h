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
#ifndef __marshal_h__
#define __marshal_h__

#include "types.h"
#include "uuid.h"

inline size_t marshalU8(uint8_t *data, uint8_t u8);
inline uint8_t unmarshalU8(const uint8_t *data);

inline size_t marshalU16(uint8_t *data, uint16_t u16);
inline uint16_t unmarshalU16(const uint8_t *data);

inline size_t marshalU32(uint8_t *data, uint32_t u32);
inline uint32_t unmarshalU32(const uint8_t *data);

#define marshalUUID(data, uuid) (memcpy((uint8_t *)(data), (uint8_t *)(uuid), sizeof(uuid_t)), sizeof(uuid_t))
#define unmarshalUUID(data, uuid) marshalUUID(uuid, data)

#if !defined(marshalUUID)
inline size_t marshalUUID(uint8_t *data, const uint8_t *uuid);
#endif
#if !defined(unmarshalUUID)
inline size_t unmarshalUUID(const uint8_t *data, uint8_t *uuid);
#endif

inline size_t marshal_p_string(uint8_t *data, const p_string_t *str);
inline size_t unmarshal_p_string(const uint8_t *data, p_string_t *str);

#endif
