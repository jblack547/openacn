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
#ifndef __acn_config_h__
#define __acn_config_h__

#include <types.h>
#include <acn_arch.h>

#define MAX_NODE_NAME_LEN 64
#define MAX_REBOOT_LEN    64

#define VALID_CONF_SIG		0x0000E117
#define NODE_CONFIG_VER		0
#define NODE_CURRENT_CONFIG_VER		1


typedef struct foreign_component_t
{
	struct foreign_component_t *next;
	uint8 cid[16];
	uint32 adhocIP;
	uint16 adhocPort;
	int adhocExpiresAt;
} foreign_component_t;



typedef struct subscription_t
{
	struct subscription_t *next;
	foreign_component_t *component;
	void* session;
} subscription_t;

typedef struct
{
	uint32 dmpAddress;
   void *ptr;
	int event_reliable;
   void* postFunction;
	subscription_t *subscriptions;
} property_t; 

typedef struct
{
	char string[36];
	uint32 numProps;
	property_t props[0];
} dcid_t;

typedef struct local_component_t
{
	struct local_component_t *next;
	cid_t cid;
	dcid_t *dcid;
	int dyn_mcast;
	int offset;
} local_component_t;

typedef void (*event_callback_t)(local_component_t *localComp, uint32_t address, uint8_t *value, uint32_t valueLength);

foreign_component_t *registerForeignComponent(uint8_t *cid);
local_component_t *registerLocalComponent(uint8_t *cid, dcid_t *dcid);

void unregisterLocalComponent(uint8_t *cid);
void unregisterForeignComponent(uint8_t *cid);

local_component_t *findLocalComponent(uint8_t *cid);
foreign_component_t *findForeignComponent(uint8_t *cid);

int getProperty(local_component_t *dst, uint32_t address, uint8_t *ptr);
int setProperty(local_component_t *dst, uint32_t address, uint8_t *data, uint32_t dataLength);

void registerEventCallback(event_callback_t callback);

void loadConfig(void);

int addSubscription(local_component_t *localComp, uint32_t address, foreign_component_t *foreignComp, void *session);
int removeSubscription(local_component_t *localComp, uint32_t address, foreign_component_t *foreignComp, void *session);
int removeAllSubscriptions(foreign_component_t *foreignComp);


void selfTest(void);

enum
{
	SUCCESS = 0,
	FAILURE = -1,
	NO_SUCH_PROP_ERROR = -2,
	WRITE_ONLY_ERROR = -3,
	READ_ONLY_ERROR = -4, 
	DATA_ERROR = -5,
	ALLOCATE_MAP_NOT_SUPPORTED = -6,
	ALLOCATE_MAP_NOT_AVAILABLE = -7,
	MAP_NOT_MAPPABLE = -8,
	MAP_NOT_ALLOCATED = -9,
	SUB_NOT_SUPPORTED = -10,
	SUB_NOT_SUPPORTED_AT_ALL = -11,
};


typedef struct
{
   uint32 configVersion;
   uint32 valid;
   uint8 backlight;
   uint32 unused[16];
	char name[MAX_NODE_NAME_LEN + 2];
}
nvcNodeData;

typedef struct
{
	uint32 hardwareVersion;
	uint32 serialNumber;
	uint32 firmwareVer;
	uint8 macAddress[6];
	char versionString[MAX_NODE_NAME_LEN + 2];
	uint8 reboot;
	uint8 upgrade;
}
volNodeData;                    /* created at runtime on each boot */

typedef struct
{
	uint32 baudRate;
	uint8 dataBits;
	uint8 stopBits;
	uint8 parity;
	uint8 flowControl;
	uint8 rxEnabled;
	uint8 txEnabled;
	uint8 txRemaining;
	uint16 txBusy;
} rs232_t;

typedef struct 
{
	rs232_t rs232;
	uint8 smpteStreamID;
	uint8 mtcStreamID;
	uint8 midiRxStreamID;
	uint8 midiTxStreamID;
	uint32 unused[16];
} gateway_prop_t;


void activateConfig();
int isEventReliable(local_component_t *comp, int address);

/* global Configuration Parmams */
extern char *firmwareFile;

nvcNodeData nvcNode;   /* stored in flash */
volNodeData volNode;   /* built at boot */
extern gateway_prop_t gatewayProps;
#endif
