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

#include "configure.h"
#include "uuid.h"
#include <ctype.h>

int textToUuid(const char *uuidText, uuid_t uuidp)
{
	uint8_t *bytp;
	uint16_t byt;

	byt = 1;	//bit provides a shift marker

	for (bytp = uuidp; bytp < uuidp + UUIDSIZE; ++uuidText)
	{
		if (*uuidText == '-') continue;	//ignore dashes
		if (isdigit(*uuidText))
		{
			byt = (byt << 4) | (*uuidText - '0');
		}
		else if (isxdigit(*uuidText))
		{
			byt = (byt << 4) | (toupper(*uuidText) - 'A' + 10);
		}
		else
		{
			while (bytp < uuidp + UUIDSIZE) *bytp++ = 0;
			return -1;	//error terminates
		}
		if (byt >= 0x100)
		{
			*bytp++ = (uint8_t)byt;
			byt = 1;	//restore shift marker
		}
	}
	//FIXME - check for terminated input string here (what termination is allowed?)
	return 0;
}

const char hexdig[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
#define tohex(nibble) hexdig[nibble]

/*
Make a string from a UUID
return pointer to end of string
*/
char *uuidToText(const uuid_t uuidp, char *uuidText)
{
	int octet;

	for(octet = 0; octet < 16; octet++)
	{
		*uuidText++ = tohex(*uuidp >> 4);
		*uuidText++ = tohex(*uuidp & 0x0f);
		++uuidp;

		switch(octet)
		{
			case 3 :
			case 5 :
			case 7 :
			case 9 :
				*uuidText++ = '-';
			default :
				break;
		}
	}
	*uuidText = '\0';	//terminate the string
	return uuidText;
}

/*
we cannot compare using U32s because there is no guarantee that the uuids are aligned
*/
int uuidEqual(uuid_t uuid1, uuid_t uuid2)
{
	int count = 16;

	while (*uuid1++ == *uuid2++) if (--count == 0) return 1;
	return 0;
}
