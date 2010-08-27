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

#tabs=2s
*/
/*--------------------------------------------------------------------*/
#include "opt.h"
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include "cid.h"
#include <ctype.h>

/* stack independent calls usually found in ctype.h*/
#ifndef isdigit
  #define isdigit(c)  ((c) >= '0' && (c) <= '9')
#endif
#ifndef isxdigit
  #define isxdigit(c)  (isdigit((c)) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#endif

const cid_t  null_cid = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/******************************************************************************/
/* Convert text based CID to uuit_t
 * Input format example: D1F6F109-8A48-4435-8157-A226604DEA89
 * Returns OK or non-zero if error in format found
 */
int textToCid(const char *cidText, cid_t cidp)
{
  uint8_t *bytp;
  uint16_t byt;

  byt = 1;  /* bit provides a shift marker */

  for (bytp = cidp; bytp < cidp + CIDSIZE; ++cidText) {
    if (*cidText == '-') continue;  /* ignore dashes */
    if (isdigit(*cidText)) {
      byt = (byt << 4) | (*cidText - '0');
    } else
      if (isxdigit(*cidText)) {
        byt = (byt << 4) | (toupper(*cidText) - 'A' + 10);
      } else {
        while (bytp < cidp + CIDSIZE) *bytp++ = 0;
        return FAIL;  /* error terminates */
      }
    if (byt >= 0x100) {
      *bytp++ = (uint8_t)byt;
      byt = 1;  /* restore shift marker */
    }
  }
  /* TODO: - check for terminated input string here (what termination is allowed?) */
  return OK;
}

/******************************************************************************/
/*
Make a string from a CID
Returns pointer to start of string
String is fixed length CID_STR_SIZE (including Nul terminator)
*/
char *cidToText(const uint8_t *cidp, char *cidText)
{
  int octet;
  char *cp;

  static const char hexdig[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
#define tohex(nibble) hexdig[nibble]

  for (cp = cidText, octet = 0; octet < 16; octet++) {
    *cp++ = tohex(*cidp >> 4);
    *cp++ = tohex(*cidp & 0x0f);
    ++cidp;

    switch(octet) {
      case 3 :
      case 5 :
      case 7 :
      case 9 :
        *cp++ = '-';
      default :
        break;
    }
  }
  *cp = '\0';  /* terminate the string */
  return cidText;
#undef tohex
}

/* Also see MACROS defined in header */
#if !defined(cidIsEqual)
/*****************************************************************************/
/*
 * Compares 2 CIDs
 * Returns non-zero if equal.
 */
int cidIsEqual(const cid_t cid1, const cid_t cid2)
{
  int count = 16;

  while (*cid1++ == *cid2++) if (--count == 0) return 1;
  return OK;
}
#endif

#if !defined(cidNull)
/*
 * Fulls CID with zeros
 */
/*****************************************************************************/
void cidNull(cid_t cid)
{
  int count = 16;

  while (count--) {
    *cid = 0;;
  }
}
#endif

#if !defined(cidIsNull)
/*****************************************************************************/
/*
 * Test CID to see if is NULL
 * Returns non-zero if NULL
 */
int cidIsNull(const cid_t cid)
{
  return cidIsEqual(cid, null_cid);
}
#endif

#if !defined(cidCopy)
/*****************************************************************************/
void cidCopy(cid_t cid1, const cid_t cid2)
{
  int count = 16;

  while (count--) {
    *cid1++ = *cid2++;
  }
}
#endif

