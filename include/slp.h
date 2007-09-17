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

	$Id: slp.h 16 2007-09-07 09:52:07Z philip $

*/
/*--------------------------------------------------------------------*/

#ifndef __slp_h__
#define __slp_h__

#include <arch/types.h>
#include <acn_config.h>
#define SLP_PORT	427
#define SLP_MCAST_ADDRESS 0xeffffffd
enum 
{
	SLP_REQUEST = 1,
	SLP_REPLY = 2,
};
enum
{
	REQUEST_FAIL = 0,
	REQUEST_PENDING = 0,
};

typedef void (slp_callback_t) (uint32 ip, uint16 port, uint16 lifetime, local_component_t *localComp, foreign_component_t *foreignComp);

typedef struct
{
	uint8  reserved;
	uint16 lifetime;
	uint16 urlLength;
	char   urlText[0];
} __attribute__ ((packed)) urlSpec_t;

typedef struct
{
	uint8 version;
	uint8 function;
	uint8 lengthHi;
	uint16 length;
	uint16 flags;
	uint8 nextExtensionOffset[3];
	uint16 transactionID;
	uint16 langTagLen;
	char   language[2];
	uint8  url[0];
}__attribute__ ((packed)) slp_t;

void initSlp(void);
void unregisterSlp(local_component_t *comp);
void registerSlp(local_component_t *comp);
void updateSlpRegistration(local_component_t *comp);
void srvRequest(char *serviceUrl, uint32 serviceUrlLength, uint32 dstIP);
uint32 findSdtAdhocPort(local_component_t *localComp, foreign_component_t *foreignComp, slp_callback_t f);
//void addRdmSlp(void);
//void delRdmSlp(void);

#endif
