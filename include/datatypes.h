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
#ifndef __datatypes_h__
#define __datatypes_h__ 1
/*
enum
{
   DATATYPE_NULL,
   DATATYPE_BOOL,
   DATATYPE_U8,
   DATATYPE_U16,
   DATATYPE_U32,
   DATATYPE_STRING,
   DATATYPE_BYTE_ARRAY,
   DATATYPE_TBMODE,
   DATATYPE_FUNC,
};
*/
#define PROP_SIZE_SHIFT		23
#define PROP_INDEX_MASK		0x0000FFFF
#define PROP_SIZE_MASK		0xFF800000
#define PROP_POST_FUNC_BIT	0x00400000
#define PROP_FIXED_LEN_BIT	0x00200000
#define PROP_PERIPH_BIT		0x00100000
#define PROP_EVENT_BIT		0x00080000
#define PROP_READ_BIT		0x00040000
#define PROP_WRITE_BIT		0x00020000
#define PROP_MAP_BIT			0x00010000

#define PROP_FIXED_LEN_4	((4 << PROP_SIZE_SHIFT) | PROP_FIXED_LEN_BIT)
#define PROP_FIXED_LEN_2	(PROP_FIXED_LEN_BIT | (2 << PROP_SIZE_SHIFT))
#define PROP_FIXED_LEN_1	((1 << PROP_SIZE_SHIFT) |PROP_FIXED_LEN_BIT)
#endif
