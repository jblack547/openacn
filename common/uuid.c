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

	$Id: uuid.c 21 2007-09-07 10:55:32Z philip $

*/
/*--------------------------------------------------------------------*/

void textToCid(const char *cidText, uint8 cid[16])
{
	uint32 nibble = 0;
	uint32 count;
	
	memset(cid, 0, 16);
	
	for(count = 0; count < 37; count++)
	{
		if(isdigit(*cidText))
		{
			cid[nibble / 2] |= ((*cidText - 48) << ((nibble % 2) ? 0 : 4));
			cidText++;
			nibble++;
		}
		else if(isalpha(*cidText))
		{
			cid[nibble / 2] |= ((toupper(*cidText) - 55) << ((nibble % 2) ? 0 : 4));
			cidText++;
			nibble++;
		}
		else if(*cidText == '-')
		{
			cidText++;
		}
		else
		{
			;//error
		}
	}
}

char *cidToText(uint8 cid[16], const char *cidText)
{
	uint32 octet;
	for(octet = 0; octet < 16; octet++)
	{
		switch(octet)
		{
			case 3 :
			case 5 :
			case 7 :
			case 9 :
				cidText += sprintf((char*)cidText, "%02X-", cid[octet]);
				break;
			default :
				cidText += sprintf((char*)cidText, "%02X", cid[octet]);
		}
	}
	return (char*)cidText;
}
