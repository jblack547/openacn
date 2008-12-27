/*--------------------------------------------------------------------*/
/*
Copyright (c) 2008, Electronic Theatre Controls, Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Electronic Theatre Controls, Inc. nor the names of its
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

  Description:
    Handy processor dependent routines to pack and unpack data structures
 */

#include <string.h>

#include "opt.h"
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"

#include "pack.h"

/* PACK ******************************************************************************** */
char *packSTR(char *charptr, char *val)
{
  int cnt;
  cnt = strlen(val);
  memcpy(charptr, val, cnt);
  return charptr + cnt;
}

char *packSTRlen(char *charptr, char *val, uint32_t len)
{
  uint32_t cnt;
  uint32_t dif;

  cnt = strlen(val);
  if (cnt >= len) {
  	memcpy(charptr, val, len);
  } else {
  	memcpy(charptr, val, cnt);
  	dif = len - cnt;
  	memset(&charptr[cnt], 0, dif);
  }
	charptr[len-1] = 0;				/* null */
  return charptr + len;
}


char *packMEM(char *charptr, char *val, uint32_t cnt)
{
  memcpy(charptr, val, cnt);
  return charptr + cnt;
}

char *packUINT8(char *charptr, uint8_t val)
{
  charptr[0] = val;
  return charptr + 1;
}

char *packUINT16(char *charptr, uint16_t val)
{
  charptr[0] = (val >> 8) & 0xff;
  charptr[1] = val & 0xff;
  return charptr + 2;
}

char *packUINT24(char *charptr, uint32_t val)
{
  charptr[0] = (val >> 16) & 0xff;
  charptr[1] = (val >> 8) & 0xff;
  charptr[2] = val & 0xff;
  return charptr + 3;
}

char *packUINT32(char *charptr, uint32_t val)
{
  charptr[0] = (val >> 24) & 0xff;
  charptr[1] = (val >> 16) & 0xff;
  charptr[2] = (val >> 8) & 0xff;
  charptr[3] = val & 0xff;
  return charptr + 4;
}

/* UNPACK ****************************************************************************** */
char *unpackUINT8(char *charptr, uint8_t *val)
{
  *val = (uint8_t)charptr[0];/* *((uint8_t*)charptr++); */
  return charptr+1;
}

char *unpackUINT16(char *charptr, uint16_t *val)
{
  *val  = (uint16_t)((uint8_t)charptr[0])<<8;
  *val |= (uint16_t)(uint8_t)charptr[1];
  return charptr + 2;
}

char *unpackUINT24(char *charptr, uint32_t *val)
{
  *val  = (uint16_t)((uint8_t)charptr[0])<<16;
  *val |= (uint16_t)((uint8_t)charptr[1])<<8;
  *val |= (uint16_t)((uint8_t)charptr[2]);
  return charptr + 3;
}

char *unpackUINT32(char *charptr, uint32_t *val)
{
  *val  = (uint32_t)((uint8_t)charptr[0])<<24;
  *val |= (uint32_t)((uint8_t)charptr[1])<<16;
  *val |= (uint32_t)((uint8_t)charptr[2])<<8;
  *val |= (uint32_t)((uint8_t)charptr[3]);
  return charptr + 4;
}
