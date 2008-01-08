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
#ifndef __dmp_h__
#define __dmp_h__ 1

#include <arch/types.h>
#include <acn_config.h>



#define ADDRESS_TYPE_MASK				0x30
#define VIRTUAL_ADDRESS_BIT			0x80
#define RELATIVE_ADDRESS_BIT			0x40
#define ADDRESS_SIZE_MASK				0x03
#define RESERVED_BIT_MASK				0x0C


#define DMP_VECTOR_LEN 1
#define DMP_HEADER_LEN 1

enum
{
	ONE_OCTET_ADDRESS = 0,
	TWO_OCTET_ADDRESS = 1,
	FOUR_OCTET_ADDRESS = 2,
	RESERVED = 3,
};

enum
{
	SINGLE_ADDRESS_SINGLE_DATA    = 0x00,
	RANGE_ADDRESS_SINGLE_DATA     = 0x10,
	RANGE_ADDRESS_EQUAL_SIZE_DATA = 0x20,
	RANGE_ADDRESS_MIXED_SIZE_DATA = 0x30,
};

enum
{
	DMP_GET_PROPERTY = 					1,
	DMP_SET_PROPERTY = 					2,
	DMP_GET_PROPERTY_REPLY =			3,
	DMP_EVENT =								4,
	DMP_MAP_PROPERTY =					5,
	DMP_UNMAP_PROPERTY =					6,
	DMP_SUBSCRIBE =						7,
	DMP_UNSUBSCRIBE =						8,
	DMP_GET_PROPERTY_FAIL = 			9,
	DMP_SET_PROPERTY_FAIL = 			10,
	DMP_MAP_PROPERTY_FAIL =				11,
	DMP_SUBSCRIBE_ACCEPT =				12,
	DMP_SUBSCRIBE_REJECT =				13,
	DMP_ALLOCATE_MAP =					14,
	DMP_ALLOCATE_MAP_REPLY =			15,
	DMP_DEALLOCATE_MAP =					16,
};

void initDmp(void);
uint32 dmpRxHandler(foreign_component_t *srcComp, local_component_t *dstComp, void *srcSession, uint8 *data, uint32 dataLen);
void dmpCompOfflineNotify(foreign_component_t *remoteComp);
#endif
