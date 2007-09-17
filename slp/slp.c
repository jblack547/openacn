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

	$Id: slp.c 8 2007-09-05 16:26:12Z philip $

*/
/*--------------------------------------------------------------------*/
#include <slp.h>

#define __WATTCP_KERNEL__ // for access to the tcpHeader
#include <wattcp.h>

#include <string.h>
#include <arch.h>
#include <ppmalloc.h>
#include <marshal.h>
#include <crond.h>
#include <syslog.h>
#include "../acn_protocols/rlp.h"

//static char urlString[100];
//static char urlString2[100];
#define MAX_REPLY_WAIT 5
#define MAX_PENDING_REQUESTS 5
#define MAX_SLP_REG 50
#define NUM_DAS 25

#define REG_LIFETIME 300 //seconds

#define SERVICE_TYPE "service:acn.esta"
#define SERVICE_TYPE_LEN 16

#define SCOPE_LIST "ACN-DEFAULT"
#define SCOPE_LIST_LEN 11


//FIXME -- we should search for stale registrations and remove them from the DAs
static udp_Socket s;

static uint8 buffer[500];

typedef struct slp_reg_t
{
	local_component_t *component;
	struct slp_reg_t *next;
	uint32 timeout;
} slp_reg_t;

typedef struct request_t
{
	struct request_t *next;
	uint16 xid;
	uint16 remainingTicks;
	slp_callback_t *callback;
	local_component_t *localComp;
	foreign_component_t *foreignComp;
} request_t;

enum
{
	
	
	SLP_SRV_REQUEST = 1,		//Required
	SLP_SRV_REPLY = 2,		//Required
	SLP_SRV_REG = 3,			//Required
	SLP_SRV_DEREG = 4,		//Required
	SLP_SRV_ACK = 5,			//Required
	SLP_ATTR_REQUEST = 6,
	SLP_ATTR_REPLY = 7,
	SLP_DA_ADVERT = 8,		//Required
	SLP_SRV_TYPE_REQUEST = 9,
	SLP_SRV_TYPE_REPLY = 10,
	SLP_SA_ADVERT = 11,
};

static request_t *pendingRequests = 0;
static slp_reg_t *registrations = 0;
static slp_reg_t regBufs[MAX_SLP_REG]; //should be linked to MAX Components or similar.
//static void slpTick(void); //FIXME -- needs to be implemented to perform retries
static int slpRxHandler(void *s, uint8 * data, int data_len, tcp_PseudoHeader * pseudo, void *hdr);
static uint32 daList[NUM_DAS];
static request_t requestMemory[MAX_PENDING_REQUESTS];
static uint16 pendingXID = 1;


static uint32 encodeUrl(uint8 *data, uint8 *cid, uint32 ip, uint16 port, uint16 lifetime);
static uint32 encodeAttribList(uint8 *data, uint8 *cid, char *dcid, uint32 ip, uint16 port, char *uacn);
static void slpTxRegistration(slp_reg_t *reg, uint32 newReg);
static void slpTick(void);

static slp_reg_t *findRegistration(local_component_t *comp)
{
	slp_reg_t *cur;	
	
	cur = registrations;
	while(cur)
	{
		if(comp == cur->component)
		{
			break;
		}
		cur = cur->next;
	}
	return cur;
}

void initSlp(void)
{ 
	uint32 timer;
	slp_t *slp;
	
//   mcast_join(SLP_MCAST_ADDRESS);
   
   memset(buffer, 0, sizeof(buffer));
   slp = (void*)buffer;
   
   slp->version = 2;
	slp->langTagLen = htons(2);
	slp->language[0] = 'e';
	slp->language[1] = 'n';
	
	//Clear the DA List
	memset(daList, 0, sizeof(uint32) *	 NUM_DAS);
	
	//open the slp port
	udp_open(&s, SLP_PORT, -1, 0, slpRxHandler);
	
	//allocate memory for slp registrations
	allocateToPool((void*)regBufs, sizeof(slp_reg_t) * MAX_SLP_REG, sizeof(slp_reg_t));
	allocateToPool((void*)requestMemory, sizeof(request_t) * MAX_PENDING_REQUESTS, sizeof(request_t));
	
	slpTick();
	timer = timer_set_ms(500);
	while(!timer_expired(timer))
	{
		tcp_tick(0);
		if(daList[0])
			break;
	}
	addTask(60000, slpTick, -1);
}

uint32 findSdtAdhocPort(local_component_t *localComp, foreign_component_t *foreignComp, slp_callback_t f)
{
	request_t *request;
	
	request = ppMalloc(sizeof(request_t));
	
	if(!request)
		return REQUEST_FAIL;
	request->next = pendingRequests;
	pendingRequests = request;
	
	request->xid = pendingXID;
	request->remainingTicks = MAX_REPLY_WAIT;
	request->localComp = localComp;
	request->foreignComp = foreignComp;
	request->callback = f;
	
	srvRequest("service:esta-acn:dmp", strlen("service:esta-acn:dmp"), 0);
	return REQUEST_PENDING;
}

void srvRequest(char *serviceUrl, uint32 serviceUrlLength, uint32 multicast)
{
	slp_t *slp;
	uint32 i;
	slp = (void*)buffer;
	uint32 currentLength  = sizeof(slp_t);
	
	slp->function = SLP_REQUEST;
	slp->transactionID = htons(pendingXID++);
//	slp->errorCode = 0;
	
	//Previous Responder List
	currentLength += marshalU16(&buffer[currentLength], 0);
	
	//service type
	currentLength += marshalU16(&buffer[currentLength], serviceUrlLength);
	memcpy(&buffer[currentLength], serviceUrl, serviceUrlLength);
	currentLength += serviceUrlLength;

	// Scope List
	currentLength += marshalU16(&buffer[currentLength], SCOPE_LIST_LEN);
	memcpy(&buffer[currentLength], SCOPE_LIST, SCOPE_LIST_LEN);
	currentLength += SCOPE_LIST_LEN;
	
	// Predicate
	currentLength += marshalU16(&buffer[currentLength], 0);
	
	//SPI
	currentLength += marshalU16(&buffer[currentLength], 0);
	
	slp->length = htons(currentLength);
	
	if(multicast)
		sock_sendto((void*)&s, buffer, currentLength, SLP_MCAST_ADDRESS, SLP_PORT);
	else
	{
		for(i = 0; i < NUM_DAS; i++)
		{
			if(daList[i])
				sock_sendto((void*)&s, buffer, currentLength, daList[i], SLP_PORT);
			else
				break;
		}
	}
}

void registerSlp(local_component_t *comp)
{
	slp_reg_t *newSlp;

	newSlp = ppMalloc(sizeof(slp_reg_t));
	if(!newSlp)
	{
		//malloc fail FIXME -- syslog ERROR
		;
	}
	newSlp->next = registrations;
	registrations = newSlp;
	newSlp->component = comp;
	slpTxRegistration(newSlp, 1);
	//FIXME -- need to add a tracker to check the ACK cycle
}

void updateSlpRegistration(local_component_t *comp)
{
	slp_reg_t *registration = findRegistration(comp);
	if(!registration)
		return;
	registration->timeout = jiffies; //mark it as timed out
	
}

static void slpTxRegistration(slp_reg_t *reg, uint32 newReg)
{
	slp_t *slp;
	uint8 *data;
	uint32 i;
	char uacn[65];
	
	slp = (void*)buffer;
	slp->function = SLP_SRV_REG;
	slp->transactionID = htons(pendingXID++);
	slp->flags = 0;
	data = slp->url;
		
	//FIXME Magic number for port - #define in acnProtocols
	data += encodeUrl(data, reg->component->cid, my_ip_addr, ACN_RESERVED_PORT, REG_LIFETIME);
	
	data += marshalU16(data, SERVICE_TYPE_LEN);
	memcpy(data, SERVICE_TYPE, SERVICE_TYPE_LEN);
	data += SERVICE_TYPE_LEN;
	
	data += marshalU16(data, SCOPE_LIST_LEN);
	memcpy(data, SCOPE_LIST, SCOPE_LIST_LEN);
	data += SCOPE_LIST_LEN;
	
	p_string_t *string = reg->component->dcid->props[0].ptr;
	
	strncpy(uacn, string->value, string->length);
	uacn[string->length] = 0; //null term
	data += encodeAttribList(data, reg->component->cid, reg->component->dcid->string, my_ip_addr, ACN_RESERVED_PORT, uacn);
//	data += marshalU16(data, 0);
	data += marshalU8(data, 0);
	
	slp->length = htons(data - buffer);
	
	for(i = 0; i < NUM_DAS; i++)
	{
		if(daList[i])
			sock_sendto((void*)&s, buffer, data - buffer, daList[i], SLP_PORT);
		else
			break;
	}
	reg->timeout = timer_set(REG_LIFETIME / 2);
}

static uint32 encodeAttribList(uint8 *data, uint8 *cid, char *dcid, uint32 ip, uint16 port, char *uacn)
{
	uint8  *attribLength;
//	uint32 octet1;
//	uint32 octet2;
//	uint32 octet3;
//	uint32 octet4;
//	uint32 temp;
	char ip_string[17];
	
	sprintf(ip_string, "%d.%d.%d.%d",(ip >> 24) & 0xFF,  (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
	attribLength = data;
	data += 2;
	data += sprintf((char*)data, "(cid=");
	cidToText(cid, (char*)data);
	data += 36;
	data += sprintf((char*)data, "),(acn-services=esta.dmp),(acn-fctn=Pathway Connectivity Inc.),(acn-uacn=%s),", uacn);
	data += sprintf((char*)data, "(csl-esta.dmp=esta.sdt/%s:%d;esta.dmp/d:",ip_string, port);
	memcpy(data, dcid, 36);
	data += 36;
	data += sprintf((char*)data, "),(device-description=$:tftp://%s/$.ddl)", ip_string);
	marshalU16(attribLength, (data - attribLength - 2));
	return data - attribLength;
}

static uint32 encodeUrl(uint8 *data, uint8 *cid, uint32 ip, uint16 port, uint16 lifetime)
{
	uint8  *urlLength;
	uint32 octet1;
	uint32 octet2;
	uint32 octet3;
	uint32 octet4;
	uint32 temp;
	
	*data = 0;
	data += 1; //reserved byte
	
	data += marshalU16(data, lifetime);
	
	urlLength = data;
	data += sizeof(uint16);
	
	octet1 = (ip >> 24) & 0xFF;
	octet2 = (ip >> 16) & 0xFF;
	octet3 = (ip >> 8) & 0xFF;
	octet4 = ip & 0xFF;
	
	temp = sprintf((char*)data, "service:acn.esta:///");
	data += temp;
	cidToText(cid, (char*)data);
	data += 36;
	
	*data = 0; //numAuths
	
	marshalU16(urlLength, temp + 36);
	
	return temp + 42;
}

void unregisterSlp(local_component_t *comp)
{
	slp_reg_t **prev;
	slp_reg_t *cur;	
	
	prev = &registrations;
	cur = registrations;
	while(cur)
	{
		if(comp == cur->component)
		{
			*prev = cur->next;
			ppFree(cur, sizeof(slp_reg_t));
			break;
		}
		prev = &cur->next;
		cur = cur->next;
	}
	//create and send SLP unRegistration to DAs if present
}


//renews the registrations and reissues requests if they are not fulfilled

static int newDA = 0;
static void slpTick(void)
{
	slp_reg_t *cur;
	
	cur = registrations;
	while(cur)
	{
		if(timer_expired(cur->timeout) | newDA)
			slpTxRegistration(cur, 0);
		cur = cur->next;
	}
	//Send a service request for Directory Agents to the Multicast Group
	srvRequest("service:directory-agent", strlen("service:directory-agent"), SLP_MCAST_ADDRESS);
	if(newDA)
	{
		addTask(60000, slpTick, -1);
		newDA = 0;
	}
}


//FIXME -- need to handle da's attributes and timeouts
static void addDA(uint32 daIP)
{
	int i;
	
	for(i = 0; i < NUM_DAS; i++)
	{
		if(daList[i] == daIP)
			break; //we already have this DA
		if(daList[i] == 0) //empty entry
		{
			daList[i] = daIP;
			addTask(1, slpTick, -1);
			newDA = 1;
			break;
		}
	}
}
static uint32 decodeUrl(uint8 *data, char **cid, uint32 *ip, uint16 *port, uint16 *lifetime)
{
	uint16 urlLength;
	uint32 octet1;
	uint32 octet2;
	uint32 octet3;
	uint32 octet4;
	uint32 urlCount;
	
	data += 1; //reserved byte
	
	*lifetime =unmarshalU16(data);
	data += sizeof(uint16);
	
	urlLength = unmarshalU16(data);
	data += sizeof(uint16);
	
	if((sscanf((char*)data, "service:esta-acn:dmp://%i.%i.%i.%i:%hi/%n", &octet1, &octet2, &octet3, &octet4, port, &urlCount) == 5))
	{
		*ip = (octet1 << 24) | (octet2 << 16) | (octet3 << 8) | octet4;
		*cid = (char*)data + urlCount;
	}
	return urlLength + 6;
}

static void processSrvReply(uint8 *data, uint32 dataLen)
{
	slp_t *slp;
	request_t *request;
	request_t **prev;
	
	uint16 xid;
	uint16 errorCode;
	uint16 urlCount;
	uint32 ip=0;
	uint16 port=0;
	uint16 lifetime=0;
	char  *cidText=0;
	uint8 cid[16];
	uint32 found = 0;
	
	slp = (void*) data;
	xid = ntohs(slp->transactionID);
	
	request = pendingRequests;
	prev = &pendingRequests;
	
	while(request)
	{
		if(request->xid == xid)
			break;
		prev = &(request->next);
		request = request->next;
	}
	if(request)
	{
		data += sizeof(slp_t);
		errorCode = unmarshalU16(data);
		data += sizeof(uint16);
		
		if(errorCode) 
			; //FIXME -- handle the error -- decode and possibly retry?
			
		urlCount = unmarshalU16(data);
		data += sizeof(uint16);
		while(urlCount)
		{
			data += decodeUrl(data, &cidText, &ip, &port, &lifetime);
			textToCid(cidText, cid);
			if(uuidEqual(cid, request->foreignComp->cid))
			{
				found = 1;
				break;
			}
			urlCount--;
		}
		if(found)
		{
			if(request->callback)
				(request->callback)(ip, port, lifetime, request->localComp, request->foreignComp);
			*prev = request->next;
			ppFree(request, sizeof(request_t));
		}
	}
}

static void processSrvAck(uint8 *data, uint32 dataLen)
{
	slp_t *slp;
	request_t *request;
	request_t **prev;
	
	uint16 xid;
//	uint16 errorCode;
//	uint16 urlCount;
	uint32 ip=0;
	uint16 port=0;
	uint16 lifetime=0;
//	char  *cidText;
//	uint8 cid[16];
	uint32 found = 0;
	
	slp = (void*) data;
	xid = ntohs(slp->transactionID);
	
	request = pendingRequests;
	prev = &pendingRequests;
	
	while(request)
	{
		if(request->xid == xid)
			break;
		prev = &(request->next);
		request = request->next;
	}
	if(request)
	{
		if(found)
		{
			if(request->callback)
				(request->callback)(ip, port, lifetime, request->localComp, request->foreignComp);
			*prev = request->next;
			ppFree(request, sizeof(request_t));
		}
	}
}

static void rxSrvRequest(uint32 srcIP, uint16 srcPort, slp_t *packet)
{
	p_string_t string;
	p_string_t prevRespList;
	p_string_t srvType;	
	uint8* data = packet->url;
	
	syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest");
	
	//process previous responder list
	data += unmarshal_p_string(data, &prevRespList);
	if(prevRespList.length)
	{	
	syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest previous responder list");
	}
	//process service type
	data += unmarshal_p_string(data, &srvType);
	if(srvType.length)
	{	
		syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest serviceType");
		;//
	}
	
	//process scope list
	data += unmarshal_p_string(data, &string);
	if(string.length >= SCOPE_LIST_LEN)
	{	
			syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest scope list");
	}
	//process predicate list
	data += unmarshal_p_string(data, &string);
	if(string.length)
	{	
			syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest predicate list");;//
	}
	//process predicate list
	data += unmarshal_p_string(data, &string);
	if(string.length)
	{	
		syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest SPI list");;//
	}
	if(data - (uint8*)packet == ntohs(packet->length))
			syslog(LOG_DEBUG|LOG_LOCAL3,"slpRxSrvRequest procesing complete");;//
}
		
static int slpRxHandler(void *s, uint8 * data, int data_len,
                         tcp_PseudoHeader * pseudo, void *hdr)
{
	slp_t *rx;
	uint16 srcPort;
	uint32 srcIP;
	
	rx = (void*)data;
/*	if(ntohs(rx->length) != data_len)
	{
		syslog(LOG_ERR|LOG_LOCAL3,"slpRxHandler: Invalid Length");
		return data_len;
	}
*/	
	srcIP = ntohl(((in_Header *) hdr)->source);
   srcPort = ntohs( ( (udp_Header*) (hdr + in_GetHdrlenBytes( (in_Header*)hdr)) )->srcPort );

	if(data_len < sizeof(slp_t))
		return data_len;
		
	switch(rx->function)
	{
		case SLP_DA_ADVERT :
			//Add the Directory Agent to the DA List
			addDA(srcIP);
			break;
		case SLP_SRV_REQUEST :
//			rxSrvRequest(srcIP, srcPort, rx);
			//Reply Code will go here once SA is implemented
			break;
		case SLP_SRV_REPLY :
			processSrvReply(data, data_len);
			break;
		case SLP_SRV_ACK :
			processSrvAck(data, data_len);
			//mark the outstanding registration as completed
			break;
	}	
		
	return data_len;
}




