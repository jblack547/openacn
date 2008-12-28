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
*/

/*
  Copyright (c) 2007 Electronic Theatre Controls, Inc
  All rights reserved. 
  
  Description:
    Header file for pack.c
*/

#ifndef PACK_H
#define PACK_H

#include "types.h"

#define MAKEIP(a,b,c,d)    (a<<24|b<<16|c<<8|d) /* in Host Order */

#ifdef __cplusplus
extern "C" {
#endif

char *packSTR(char *charptr, char *val);
char *packSTRlen(char *charptr, char *val, uint32_t len);
char *packMEM(char *charptr, char *val, uint32_t cnt);
char *packUINT8(char *charptr, uint8_t val);
char *packUINT16(char *charptr, uint16_t val);
char *packUINT24(char *charptr, uint32_t val);
char *packUINT32(char *charptr, uint32_t val);

char *unpackUINT8(char *charptr, uint8_t *val);
char *unpackUINT16(char *charptr, uint16_t *val);
char *unpackUINT24(char *charptr, uint32_t *val);
char *unpackUINT32(char *charptr, uint32_t *val);

#ifdef __cplusplus
}
#endif

#endif /* PACK_H */
