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
  This module creates a SLP inteface. While much of it is generic, there are parts that have been hard coded
  to work in a ACN environment

  Author:
  Bill Florac - Electroinc Theatre Controls, Inc

  Current implementation:  
  Both Service Agent (SA) and User Agent (UA) are supported. Directory Agent (DA)
  is not supported. SA and UA modes can be used at the same time.

  The functions marked FUNCTION TESTED have been tested to where I know they at least
  don't crash and seem to work correcly but it does not mean they are ready for prime time.

  Limitations: 
  SA mode does not support operation without a DA available. Parts of the code needed
  for this are in place but they have not been completed nor tested.

  Notes
  Do NOT use string functions on SLPString structures, they do NOT contain a terminating NULL!
  use getSLP_STR on it first to malloc and copy to a new string.
  
  SLP requires a slp_tick() to be called a 1 ms intervals.  

  This software was designed using the UDP layer of the LWIP TCP/IP stack (http://savannah.nongnu.org/projects/lwip/).
  This software was designed using FREERTOS (http://savannah.nongnu.org/projects/lwip/).
  This software was designed using Atmel AT91SAM7X-EX evaluation kit utilizing the AT91SAM7X256 ARM processor

  TODO:
  - Verifiy operation as network goes away and comes back...(could have new IP address!)
  - As a UA, if we are looking for a service, should we change DA each time just in case 
    they are out of sync with other DA's?
  - What to do with authentication. Ignore, discard packet? SLP_AUTH
  - Get rid of malloc and make statics? Change from mem_malloc to malloc if needed.
  - Older versions of DA's using OpenSLP would send out Unsolicited DAadverts with xid != 0. This
    has been fixed so we can remove our special handling.
  - review use of int vs. uint32_t for portability
  - review and perhaps converto to macros areas that are OS dependent (i.e. delays)
  - packing and unpacking needs to deal with big/little endian.
  - ntoa needs conditional


  Versions:
  0.0.1   Initial release to test integration

*/

/* Library includes*/
#include <stdlib.h>
#include <string.h>

/* RTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* lwIP includes. */
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/igmp.h"
#include "lwip/udp.h"

/* ACN includes */
#include "user_opt.h"
#include "types.h"
#include "uuid.h"
#include "acnlog.h"
#include "pack.h"     // to pack and unpack data strucures
#include "ntoa.h"     // "32 bit" network address value to ascii
#include "acn_port.h"

/* SLP include */
#include "slp.h"      // This module

/*=========================================================================*/
/* defines */
/*=========================================================================*/
// maximim number of directory agents to track
#define MAX_DA      10    
// allows porting
#define SLP_MALLOC(x)  malloc(x)
#define SLP_FREE(x)    free(x)

// Just a quick way to test memory allocation types
#define PBUF_TYPE PBUF_POOL

/*=========================================================================*/
/* constants */
/*=========================================================================*/
const char      acn_scope_str[]   = "ACN-DEFAULT";
const uint16_t  acn_scope_len     = sizeof(acn_scope_str) - 1;
const char      local_lang_str[]  = "en";
const uint16_t  local_lang_len    = sizeof(local_lang_str) - 1;

const char      da_req_str[]      = SLP_DA_SERVICE_TYPE;
const uint16_t  da_req_len        = sizeof(da_req_str) - 1;
const char      sa_reg_str[]      = SLP_SA_SERVICE_TYPE;
const uint16_t  sa_reg_len        = sizeof(sa_reg_str) - 1;

/*=========================================================================*/
/* globals */
/*=========================================================================*/

/*=========================================================================*/
/* locals */
/*=========================================================================*/
//xTaskHandle slp_task = NULL;    // SLP Thread createed in slp_open

struct udp_pcb *slp_pcb;        // common Protocol Control Block
struct ip_addr slpmcast;        // multicast address for SLP

uint16_t      xid;              // sequential transaction id
SLPDa_list    da_list[MAX_DA];  // note, this is fixed count!
SLPdda_timer  dda_timer;        // timer to manage finding Directory Agents
char         *attr_list;        // attribute list
int           attr_list_len;    // attribute list length ( so we don't have to recount this often)
char         *srv_url;          // service type URL
int           srv_url_len;
char         *srv_type;         // service type
int           srv_type_len;

uint32_t      msticks;          // tick counter (used for some debugging)

// callback for srvrqst
// NOTE! We only allow one active callback for each of these!
#if SLP_IS_UA
static void (*srvrqst_callback) (int error, char *url) = NULL;
static void (*attrrqst_callback) (int error, char *attr_list) = NULL;

uint16_t      srvrqst_xid = 0;  // xid of service request sent (not including da search)
uint16_t      attrrqst_xid = 0; // xid of attribute request sent
#endif

/*=========================================================================*/
/* local declarations */ 
/*=========================================================================*/
void      da_ip_list(char *str);

#if SLP_IS_SA
SLPError  da_service_ack(unsigned long ip);
void      da_reg_idx(int idx);
void      da_dereg(void);
void      da_reg(void);
#endif

#if SLP_IS_SA || SLP_IS_UA
SLPError  da_add(SLPDAAdvert *daadvert);
SLPError  da_delete(unsigned long ip);
int       da_count(void);
void      da_clear(void);
void      da_close(void);
#endif

// more packing/unpacking
char *unpackSLP_STR(char *data, SLPString *slp_str);
char *packSLP_STR(char *data, SLPString *slp_str);
char *packSLP_HEADER(char* data, SLPHeader *header);
char *unpackSLP_HEADER(char* data, SLPHeader *header);
char *getSLP_STR(SLPString *slp_str);
char *packSLP_URL_ENTRY(char* data, SLPUrlEntry *url_entry);
char *unpackURL_ENTRY(char* data, SLPUrlEntry *urlentry);

// call back for UDP receive
static void slp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port);

// UA and SA functions
#if SLP_IS_UA || SLP_IS_SA
SLPError slp_receive_daadvert(unsigned long ip, SLPHeader *header, char *data);
SLPError slp_discover_da(void);
//void     slp_active_discovery_start(void);  // see header file
#endif

// UA only functions (also see header file)
#if SLP_IS_UA
//SLPError slp_send_srvrqst(unsigned long ip, char *req_srv_type, char *reg_predicate,
//  void (*callback) (int error, char *url));
SLPError slp_receive_srvrply(unsigned long ip, SLPHeader *header, char *data);
//SLPError slp_send_attrrqst(unsigned long ip, char *req_url, char *tags,
//  void (*callback) (int error, char *attributes));
SLPError slp_receive_attrrply(unsigned long ip, SLPHeader *header, char *data);
#endif

// SA only functions
#if SLP_IS_SA
SLPError slp_send_saadvert(unsigned long ip, uint16_t reply_xid);
SLPError slp_send_reg(unsigned long ip, bool fresh);
SLPError slp_send_dereg(unsigned long ip);
SLPError slp_receive_srvrqst(unsigned long ip, SLPHeader *header, char *data);
SLPError slp_send_srvrply(unsigned long ip, uint16_t reply_xid, uint16_t error_code);
SLPError slp_receive_svrack(unsigned long ip, SLPHeader *header, char *data);
#endif

// DA only functions (not supported)
#if SLP_IS_DA
SLPError  slp_send_svrack(unsigned long ip, uint16_t reply_xid, uint16_t error_code);
#endif

/****************************************************************/
// TODO: move this to utility
// This function compares the two strings, disregarding case. 
// note: GCC has a builtin function of the same than can be access via __builtin_strcasecmp()
static int
strcasecmp (const char *s1, const char *s2)
{
  char  c1, c2;
  int    result = 0;

  while (result == 0)
  {
    c1 = *s1++;
    c2 = *s2++;
    if ((c1 >= 'a') && (c1 <= 'z'))
      c1 = (char)(c1 - ' ');
    if ((c2 >= 'a') && (c2 <= 'z'))
      c2 = (char)(c2 - ' ');
    if ((result = (c1 - c2)) != 0)
      break;
    if ((c1 == 0) || (c2 == 0))
      break;
  }
  return result;
}


/*
The following is a list of the major SLP API function calls of openSLP:
// Open handle
SLPError slp_open(const char* lang, SLPBoolean isasync, SLPHandle* phslp) 

// Close handle
void slp_close(SLPHandle hslp) 

// Registers a service URL and service attributes with SLP. 
SLPError SLPAPI slp_reg(SLPHandle   hSLP,
                const char  *srvUrl,
                const unsigned short lifetime,
                const char  *srvType,
                const char  *attrList,
                SLPBoolean fresh,
                SLPRegReport callback,
                void *cookie)

// Deregisters a previously registered service. 
SLPError slp_dereg( SLPHandlehslp, 
                   const char* srvurl, 
                   SLPRegReport callback, 
                   void* cookie ) 

// Finds services based on service type or attributes. 
SLPError SLPFindSrvs( SLPHandle hslp, 
                      const char* srvtype, 
                      const char* scopelist, 
                      const char* filter, 
                      SLPSrvURLCallback callback, 
                      void* cookie) 

// Obtains a list of attributes for services registered with SLP. 
SLPError SLPFindAttrs( SLPHandle hslp, 
                       const char* srvurl, 
                       const char* scopelist, 
                       const char* attrids, 
                       SLPAttrCallback callback, 
                       void* cookie) 

//Obtains a list of the types of services that have been registered with SLP.  
SLPError SLPFindSrvTypes( SLPHandle hslp, 
                          const char* namingauthority, 
                          const char* scopelist, 
                          SLPSrvTypeCallback callback, 
                          void* cookie) 
*/

/*============================================================================*/
// Some routines to manage our DA (directory agent) list
// We manage a limited amount of Directory agents
/*============================================================================*/


/******************************************************************************/
// desc   : return a string listing all our DAs
// params : str - pointer to start of string
// returns: void
void da_ip_list(char *str)
{
  int       x;
  uint32_t  ip;
  int       first = SLP_TRUE;
  
  str[0] = 0;
  for (x=0;x<MAX_DA;x++) {
    ip = da_list[x].ip;
    if (ip) {
      if (first) {
        sprintf(str, "%s", ntoa(ip));
      } else {
        sprintf(str, "%s,%s", str, ntoa(ip));
      }
      first = SLP_FALSE;    
    }
  }
}

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
// desc   : clears DA list
// params : none
// returns: void
// note   : does not do deregister them
void da_clear(void)
{
  int x;
  int protect;
  protect = ACN_PORT_PROTECT();
  for (x=0;x<MAX_DA;x++) {
    da_delete(da_list[x].ip);
  }
  ACN_PORT_UNPROTECT(protect);
}
#endif //SLP_IS_SA || SLP_IS_UA

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
// desc   : close and delete or DAs
// params : none
// returns: void
void da_close(void)
{
  #if SLP_IS_SA
  // deregister all
  da_dereg();
  #endif
  
  // now nuke them
  da_clear();
  
  return;
}
#endif //#SLP_IS_SA || SLP_IS_UA

#if SLP_IS_SA
/******************************************************************************/
// desc   : process registration ack for a directory agent
// params : ip - 32 bit ip address to ack to
// returns: SLP_OK if OK, or SLP_DA_NOT_FOUND if address was not in our list
SLPError da_service_ack(unsigned long ip)
{
  int x;
  
  for (x=0;x<MAX_DA;x++) {
    // see if we have it
    if (da_list[x].ip == ip) {
      switch (da_list[x].state) {
        case DA_WAIT_REG:
          // we found it so now put start the expire timer
          da_list[x].state = DA_EXPIRE;
          da_list[x].counter = SLP_REGIGISTION_REFRESH;
          da_list[x].retries = 0;
          da_list[x].timeout = CONFIG_DA_BEAT;
          break;
        case DA_WAIT_DEREG:
          // we found it mark it closed
          da_list[x].state = DA_CLOSED;
          break;
      }
      return SLP_OK;
    }
  }
  return SLP_DA_NOT_FOUND;  
}
#endif //#SLP_IS_SA

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
// desc   : adds DA to out list
// params : daadvert - advertisement structure
// returns: SLP_OK if OK, or SLP_DA_LIST_FULL list is full
SLPError da_add(SLPDAAdvert *daadvert)
{
  int     x;
  char    *ip_str;
  unsigned long ip;
  
  // Get ip
  ip_str = getSLP_STR(&daadvert->url);
  if (ip_str) {
    // convert string to ip (string MUST be: service:directory-agent://<ip>)
    ip = inet_addr(&ip_str[da_req_len + 3]);
    SLP_FREE(ip_str);
  } else
    ip = -1;
  
  // see if we have ip address
  for (x=0;x<MAX_DA;x++) {
    if (da_list[x].ip == ip) {
      if (daadvert->boot_time == 0) {
        // da is going down, just nuke it
        da_delete(da_list[x].ip);
        return SLP_OK;
      }
      // see if has rebooted
      if (daadvert->boot_time == da_list[x].boot_time) {
        // already have this one, do nothing...well...ok, mark it as still active.
        da_list[x].timeout = CONFIG_DA_BEAT;
        return SLP_OK;
      } else {
        // must of rebooted, nukeit to force a registration
        da_delete(da_list[x].ip);
      }
    } 
    // else find first free slot
    if (da_list[x].ip == 0) {
      da_list[x].ip = ip;
      da_list[x].state = DA_CLOSED;
      da_list[x].timeout = CONFIG_DA_BEAT;
      // if we have an attribute list, then set it to register
      #if SLP_IS_SA
      if (attr_list) {
        da_reg_idx(x);
      }
      #endif
      da_list[x].boot_time = daadvert->boot_time;
      return SLP_OK;
    }
  }
  // we should only get here if we can't add a new one
  return SLP_DA_LIST_FULL;
}
#endif //SLP_IS_SA || SLP_IS_UA

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
// desc   : remove a DA from our list
// params : ip - 32 bit IP address
// returns: SLP_OK if OK, or SLP_DA_NOT_FOUND if not found
SLPError da_delete(unsigned long ip)
{
  int  x;

  for (x=0;x<MAX_DA;x++) {
    if(da_list[x].ip == ip) {
      
      da_list[x].state = DA_CLOSED;
      da_list[x].ip = 0;

      return SLP_OK;
    }
  }
  return SLP_DA_NOT_FOUND;
}
#endif //SLP_IS_SA || SLP_IS_UA

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
// desc   : count DAs
// params : none
// returns: number of DAs found
int da_count(void) 
{
  int  x;
  int result = 0;

  for (x=0;x<MAX_DA;x++) {
    if(da_list[x].ip) {
      result ++;
    }
  }
  return result;
}
#endif //#SLP_IS_SA || SLP_IS_UA


#if SLP_IS_SA
/******************************************************************************/
// desc   : tell a DA about our registration
// params : idx - index to da list
// returns: void
// note   : does not verify idx nor if there is an ip in list
void da_reg_idx(int idx)
{
  uint16_t  tps_delay;

  // get a random delay 
  tps_delay = rand();
  while (tps_delay > CONFIG_REG_ACTIVE ) {
    tps_delay = tps_delay >> 1;
  }

  da_list[idx].state = DA_SEND_REG;
  da_list[idx].counter = tps_delay;
  da_list[idx].retries = 2;
}
#endif //SLP_IS_SA

#if SLP_IS_SA
/******************************************************************************/
// desc   : tell DAs about our registration
// params : none
// returns: void
void da_reg(void) 
{
  int  x;
  for (x=0;x<MAX_DA;x++) {
    if(da_list[x].ip) {
      da_reg_idx(x);
    }
  }
}
#endif //SLP_IS_SA

#if SLP_IS_SA
/******************************************************************************/
// desc   : tell da about our deregistration
// params : none
// returns: void
void da_dereg(void) 
{
  int x;
  int  valid;
  
  for (x=0;x<MAX_DA;x++) {
    if (da_list[x].ip) {
      da_list[x].state = DA_SEND_DEREG;
      da_list[x].counter = CONFIG_RETRY;
      da_list[x].retries = 2;
    }
  }
  
  // now wait for all of them to close
  // perhaps this should be non-blocking
  // TODO: possbible endless loop 
  do {
    vTaskDelay(500/portTICK_RATE_MS);
    valid = 0;
    for (x=0;x<MAX_DA;x++) {
      if (da_list[x].state != DA_CLOSED) {
        valid = 1;
        break;
      }
    }
  } while (valid);
  
}
#endif // SLP_SA



/*============================================================================*/
// Some SLP specific packing functions
// These should be processor independent and use the processor dependent
// pack.c file to do the actual parsing
/*============================================================================*/

/*******************************************************************************
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <string>        |           <string>            \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
// desc   : unpack a SLP string containing a length and string data
// params : data, pointer to start of string
//        : slp_str, structure to write
// returns: pointer past end of string
//
// NOTE: The string data is pointed into the buffer and NOT copied!
//       this avoid having to alloc but one can NOT use str functions
//       as there is no termination null
/******************************************************************************/
char *unpackSLP_STR(char *data, SLPString *slp_str)
{
  data = unpackUINT16(data, &slp_str->len);
  if (slp_str->len > 0) {
    slp_str->str = data;
    data += slp_str->len;
  }
  return (data);
}

/******************************************************************************/
// desc   : Unpack a SLPString to a real string. 
// params : slp_str - pointer to SLPString structure
// note   : YOU MUST FREE THIS YOURSELF WITH SLP_FREE()
/******************************************************************************/
char *getSLP_STR(SLPString *slp_str) 
{
  char *result;
  int  len;
 
  // get length
  len = slp_str->len;
  // small check on length
  if (len > netif_default->mtu)
    len = 0;
  result = SLP_MALLOC(len+1);
  if (result) {
    // copy to a real string
    memcpy(result, slp_str->str, len);
    result[len] = 0;
  }; 
  return result;
}

/******************************************************************************/
// desc   : pack a SLP string containing a length and string data
// params : data, pointer to start of string
//        : slp_str, structure to read
// returns: pointer past end of string
//        : string structure gets put into data
/******************************************************************************/
char *packSLP_STR(char *data, SLPString *slp_str)
{
  data = packUINT16(data, slp_str->len);
  if (slp_str->len > 0) {
    memcpy(data, slp_str->str, slp_str->len);
    data += slp_str->len;
  }
  return (data);
}

/*******************************************************************************
  SLP messages all begin with the following header:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    Version    |  Function-ID  |            Length             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | Length, contd.|O|F|R|       reserved          |Next Ext Offset|
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  Next Extension Offset, contd.|              XID              |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      Language Tag Length      |         Language Tag          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
// desc   : pack a SLP header
// params : data, points to start of header
//          header, structure to write
// returns: pointer past end of header
//          header stucture get put into data
/******************************************************************************/
char *packSLP_HEADER(char* data, SLPHeader *header) 
{
  data = packUINT8(data, header->version);       // version
  data = packUINT8(data, header->function_id);   // function-id
  data = packUINT24(data, header->length);       // length
  data = packUINT16(data, header->flags);        // flags
  data = packUINT24(data, header->ext_offset);   // Next Ext Offset
  data = packUINT16(data, header->xid);          // XID
  data = packSLP_STR(data, &header->lang_tag);   // Language Tag Length
  return (data);
}

/******************************************************************************/
// desc   : unpack a SLP header
// params : data, points to start of header
//          header, header structure to place data
// returns: pointer past end of header
//        : modifies header structure
/******************************************************************************/
char *unpackSLP_HEADER(char* data, SLPHeader *header) 
{
  data = unpackUINT8(data, &header->version);       // version
  data = unpackUINT8(data, &header->function_id);   // function-id
  data = unpackUINT24(data, &header->length);       // length
  data = unpackUINT16(data, &header->flags);        // flags
  data = unpackUINT24(data, &header->ext_offset);   // Next Ext Offset
  data = unpackUINT16(data, &header->xid);          // XID
  data = unpackSLP_STR(data, &header->lang_tag);    // Language Tag
  return (data);
}

/*******************************************************************************
4.3. URL Entries
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   Reserved    |          Lifetime             |   URL Length  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |URL len, contd.|            URL (variable length)              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |# of URL auths |            Auth. blocks (if any)              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
******************************************************************************/
// desc   : unpack a SLP header
// params : data, points to start of header
//          header, header structure to place data
// returns: pointer past end of header
//        : modifies header structure
/******************************************************************************/
char *packSLP_URL_ENTRY(char* data, SLPUrlEntry *urlentry) 
{
  // url-entry
  data = packUINT8(data, 0);                        // Reserved
  data = packUINT16(data, urlentry->lifetime);      // Lifetime
  data = packSLP_STR(data, &urlentry->url);          // url
  data = packUINT8(data,0);                         // # of URL Auths (not supported)
  return (data);
}

// return URL from data
// note: if number of authentication blocks is not NULL, the pointer data, will be void
// and the rest of the packet discarded.
/*******************************************************************************
4.3. URL Entries
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   Reserved    |          Lifetime             |   URL Length  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |URL len, contd.|            URL (variable length)              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |# of URL auths |            Auth. blocks (if any)              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
******************************************************************************/
// desc   : unpack a SLP url entry
// params : data, points to start of url entry
//          urlentry, urlentry structure to place data
// returns: pointer past end of urlentry
//        : modifies urlentry structure
// notes  : if number of authentication blocks is not NULL, the pointer data, will be void
//          and the rest of the packet discarded
/******************************************************************************/
char *unpackURL_ENTRY(char* data, SLPUrlEntry *urlentry) 
{
  uint8_t dummy;  

  data = unpackUINT8(data, &dummy);  // reserved, toss this out
  data = unpackUINT16(data, &urlentry->lifetime);
  data = unpackSLP_STR(data, &urlentry->url);
  data = unpackUINT8(data, &urlentry->num_auth_blocks);
  return (data);
}

/******************************************************************************/
// Check and process DA timers
// called once per SLP_TMR_INTERVAL
void slp_tick(void *arg)
{
//  static int second_counter = 0;
  int  x;
  int  protect;

  SLP_UNUSED_ARG(arg);

  msticks += SLP_TMR_INTERVAL;
  
  // see if we need to send DA discovers
  if (dda_timer.discover) {
    if (dda_timer.counter == 0) {
      // turn it off if we have sent three
      if (dda_timer.xmits == 1) {
        // set it up for next hit
        dda_timer.counter = CONFIG_DA_FIND;
        dda_timer.xmits = 0;
        dda_timer.xid = 0;
      } else {
        dda_timer.counter = CONFIG_MC_MAX;
        dda_timer.xmits++;
        dda_timer.xid = xid;
        #if SLP_IS_UA || SLP_IS_SA
        slp_discover_da();
        #endif
        // only send once per loop
        return;
      }
    } else {
      dda_timer.counter -= SLP_TMR_INTERVAL;
      if ((sint32_t)dda_timer.counter < 0) {
        dda_timer.counter = 0;
      }
    }
  }
  for (x=0;x<MAX_DA;x++) {
    if (da_list[x].ip) {
      // deal with DA timeout first
      da_list[x].timeout--;
      if (!da_list[x].timeout) {
        acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: da_timeout:[%s]", ntoa(da_list[x].ip)); 
        protect = ACN_PORT_PROTECT();
        da_delete(da_list[x].ip);
        ACN_PORT_UNPROTECT(protect);
        continue;
      }
      if (da_list[x].counter == 0) {
        switch (da_list[x].state) {
          case DA_SEND_REG:
            // time to send registration
            da_list[x].state = DA_WAIT_REG; 
            da_list[x].counter = CONFIG_RETRY;
            da_list[x].retries = 2;
            #if SLP_IS_SA
            slp_send_reg(da_list[x].ip, SLP_TRUE);
            #endif
            // only send once per loop
            return;
            //break;
          case DA_WAIT_REG:
            if (da_list[x].retries) {
              // try again
              da_list[x].counter = CONFIG_RETRY;
              da_list[x].retries--;
              #if SLP_IS_SA
              slp_send_reg(da_list[x].ip, SLP_TRUE);
              #endif
              // only send once per loop
              return;
            } else {
              // time out on registration so remove it
              da_list[x].ip = 0;
            }
            break;
          case DA_EXPIRE:
            // time out expired so we need to refresh registration
            da_list[x].counter = SLP_REGIGISTION_REFRESH;
            da_list[x].retries = 0;
            #if SLP_IS_SA
            slp_send_reg(da_list[x].ip, SLP_FALSE);
            #endif
            // only send once per loop
            return;
            //break;
          case DA_SEND_DEREG:
            // send a deregistration
            da_list[x].state = DA_WAIT_DEREG;
            da_list[x].counter = CONFIG_RETRY;
            da_list[x].retries = 2;
            #if SLP_IS_SA
            slp_send_dereg(da_list[x].ip);
            #endif
            // only send once per loop
            return;
            //break;
          case DA_WAIT_DEREG:
            if (da_list[x].retries) {
              // try again
              da_list[x].counter = CONFIG_RETRY;
              da_list[x].retries--;
              #if SLP_IS_SA
              slp_send_dereg(da_list[x].ip);
              #endif
              // only send once per loop
              return;
            } else {
              // time out on de-registration so remove it
              ACN_PORT_PROTECT();
              da_delete(da_list[x].ip);
              ACN_PORT_UNPROTECT(protect);
            }
            break;
        }
      } else {
        da_list[x].counter--;
      } // of counter == 0
    } // of has ip
  } // of for
}  

#if SLP_IS_UA
/*******************************************************************************
  8.1. Service Request
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Service Location header (function = srvrqst = 1)        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      length of <PRList>       |        <PRList> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   length of <service-type>    |    <service-type> String      \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    length of <scope-list>     |     <scope-list> String       \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of predicate string   |  Service Request <predicate>  \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of <SLP SPI> string   |       <SLP SPI> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
// desc   : sends a service request
// params : ip - IP to send it to
// returns: SLP_OK if sent
//        : error message if fail
/******************************************************************************/
//FUNCTION TESTED
SLPError slp_send_srvrqst(unsigned long ip, char *req_srv_type, char *reg_predicate,
  void (*callback) (int error, char *url))
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;
    
  SLPString   slp_string;
  SLPHeader   slp_header;

  // if ip = 0, then use first from list
  if (!ip) {
    if (!da_list[0].ip) {
      return(SLP_PARAMETER_BAD);
    } 
    ip = da_list[0].ip;
  }
  
  acnlog(LOG_DEBUG | LOG_SLP , "slp_send_srvrqst");  

  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);

  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP, "slp_send_srvrqst: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;
  
  // add header
  slp_header.version = SLP_VERSION;           // version
  slp_header.function_id = SLP_FUNCT_SRVRQST; // request
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                  // Next Ext Offset
  slp_header.xid = xid++;                     // XID
  slp_header.lang_tag.str = (char*)local_lang_str;
  slp_header.lang_tag.len = local_lang_len;
  offset = packSLP_HEADER(offset, &slp_header);

  // TODO: If running without DA,
  // we need to make a SA list of SA's we have
  // This then would also go out multicast
  // PRList
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);
  
  // service-type
  if (req_srv_type) {
    slp_string.len = strlen(req_srv_type);
  } else {
    slp_string.len = 0;
  }
  slp_string.str = req_srv_type;
  offset = packSLP_STR(offset, &slp_string);
  
  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);
  
  /// predicate string
  if (reg_predicate) {
    slp_string.len = strlen(reg_predicate);
  } else {
    slp_string.len = 0;
  }
  slp_string.str = reg_predicate;
  offset = packSLP_STR(offset, &slp_string);

  // slp_spi (null)
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);
   
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
  
  srvrqst_callback = callback;
  srvrqst_xid = slp_header.xid;
   
  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &ipaddr, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif  //SLP_IS_UA

#if SLP_IS_SA
/*******************************************************************************
  8.1. Service Request
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Service Location header (function = srvrqst = 1)        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      length of <PRList>       |        <PRList> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   length of <service-type>    |    <service-type> String      \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    length of <scope-list>     |     <scope-list> String       \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of predicate string   |  Service Request <predicate>  \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of <SLP SPI> string   |       <SLP SPI> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : process a service request
          Normally, a UA will get a services from a DA but if a DA is not found,
          a UA may multicast a request to DA. This routine handles that request
          by sending a unicast reply
 params : ip - IP of sender
        : header, parsed SLP header
        : data, pointer to start of data block
 returns: SLP_OK if valid
        : non-zero if not valid
*******************************************************************************/
//FUNCTION NOT TESTED (for use without DA)
// TODO, compare needed to be looked at.
// We need to check on partial compare on service type'
// we need to compare on predicate string
SLPError slp_receive_srvrqst(unsigned long ip, SLPHeader *header, char *data)
{
  SLPSrvRequest srv_req;
  char *our_ip;
  char *pr_list;
  char *scope_list;
  char *service_type;
  
  data = unpackSLP_STR(data, &srv_req.pr_list);
  data = unpackSLP_STR(data, &srv_req.service_type);
  data = unpackSLP_STR(data, &srv_req.scope_list);
  data = unpackSLP_STR(data, &srv_req.predicate);
  data = unpackSLP_STR(data, &srv_req.slp_spi);
  
  if (srv_req.slp_spi.len != 0) {
    slp_send_srvrply(ip, header->xid, SLP_AUTHENTICATION_UNKNOWN);
    return SLP_OK;
  }
  
  our_ip = ntoa(netif_default->ip_addr.addr);
  
  pr_list = getSLP_STR(&srv_req.pr_list);
  
  if (pr_list) {
    // check to see if we are prlist (else don't reply)
     if (!strstr(pr_list, our_ip)) { 
      SLP_FREE(pr_list);
      scope_list = getSLP_STR(&srv_req.scope_list); 
      if (scope_list) {
        // check to see if scope is correct or blank
        if (scope_list[0] == 0 || (!strcasecmp(scope_list, acn_scope_str))) {
          SLP_FREE(scope_list);
          service_type = getSLP_STR(&srv_req.service_type); 
          if (service_type) {
            // check service type
            if (!strcasecmp(sa_reg_str, service_type)) {
              SLP_FREE(service_type);
              // SEND SAAdvert
              slp_send_saadvert(ip, header->xid);
            } else {
              if (!strcasecmp(srv_type, service_type)) {
                // yep, that's us...
                SLP_FREE(service_type);
                if (!(header->flags & SLP_FLAG_MCAST)) {
                  slp_send_srvrply(ip, header->xid, SLP_OK);
                }
                // SEND SAAdvert here
              } else {
                // not our service so just exit out
                SLP_FREE(service_type);
              }
            }
          } else {
            return SLP_MEMORY_ALLOC_FAILED;
          }
        } else {
          // wrong scope, if unicast report error
          SLP_FREE(scope_list);
          if (!(header->flags & SLP_FLAG_MCAST)) {
           slp_send_srvrply(ip, header->xid, SLP_SCOPE_NOT_SUPPORTED);
          }
        }
      } else {
        return SLP_MEMORY_ALLOC_FAILED;
      }
    } else  {
      // we are in prlist so just exit out
      SLP_FREE(pr_list);
    }
  } else {
    return SLP_MEMORY_ALLOC_FAILED;
  }
  return SLP_OK;
}
#endif

#if SLP_IS_UA || SLP_IS_SA
/*******************************************************************************
  8.1. Service Request
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Service Location header (function = srvrqst = 1)        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      length of <PRList>       |        <PRList> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   length of <service-type>    |    <service-type> String      \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    length of <scope-list>     |     <scope-list> String       \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of predicate string   |  Service Request <predicate>  \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of <SLP SPI> string   |       <SLP SPI> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send a service request for da discovery
 params : none
 returns: 0 if valid
        : error message from udp_send()
 note   : this is similar to but sends multicast
******************************************************************************/
//FUNCTION TESTED
SLPError slp_discover_da(void)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  
  SLPString   slp_string;
  SLPHeader   slp_header;
  
  acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: xid=%d", xid);  
  
  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;           // version
  slp_header.function_id = SLP_FUNCT_SRVRQST; // request
  slp_header.flags = SLP_FLAG_MCAST;          // set multicast flag
  slp_header.ext_offset = 0;                  // Next Ext Offset
  slp_header.xid = xid++;                     // XID
  slp_header.lang_tag.str = (char*)local_lang_str;
  slp_header.lang_tag.len = local_lang_len;
  offset = packSLP_HEADER(offset, &slp_header);

  // PRList
  slp_string.str = SLP_MALLOC(MAX_DA*17);      // 16 bytes and a comma per item + null
  if (slp_string.str) {
    da_ip_list(slp_string.str);
    slp_string.len = strlen(slp_string.str);
    offset = packSLP_STR(offset, &slp_string);
    acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: [%s]",slp_string.str);  
    SLP_FREE(slp_string.str);
  } else { 
    acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }
    
  // service-type
  slp_string.len = da_req_len;
  slp_string.str = (char*)da_req_str;
  offset = packSLP_STR(offset, &slp_string);
  
  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);
  
  // predicate string(null)
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  // slp_spi (null)
  offset = packSLP_STR(offset, &slp_string);
   
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
   
  // send it
  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif

#if SLP_IS_SA
/*******************************************************************************
  8.2. Service Reply
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Service Location header (function = srvrply = 2)       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Error Code             |        URL Entry count        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       <URL Entry 1>          ...       <URL Entry N>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : sends a reply to a service request
 params : ip - address to send it to
        : header - parsed SLP header
        : xid - XID to use
        : error_code - error code to put in the message
 returns: SLP_OK if sent
******************************************************************************/
//FUNCTION NOT TESTED (for use without DA)
SLPError slp_send_srvrply(unsigned long ip, uint16_t reply_xid, uint16_t error_code) 
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;
  
  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;

  acnlog(LOG_DEBUG | LOG_SLP, "slp_send_srvrply");  
  // if we have and error code, we send it but no URL entry
  
  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP, "slp_send_srvrply: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;
 
  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVRPLY;       // reply
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = reply_xid;                       // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);
  
  // Error code
  offset = packUINT16(offset, error_code);

  // pack URL unless there is an error
  if (!error_code) {
    // pack URL count (we only have one)
    offset = packUINT16(offset,1);
    // pack URL
    slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
    slp_urlentry.url.len = srv_url_len;
    slp_urlentry.url.str = srv_url;
    slp_urlentry.num_auth_blocks = 0;
    offset = packSLP_URL_ENTRY(offset, &slp_urlentry);
  } else {
    offset = packUINT16(offset,0);
  }
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
   
  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //SLP_IS_SA

#if SLP_IS_UA
/*******************************************************************************
  8.2. Service Reply
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Service Location header (function = srvrply = 2)       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Error Code             |        URL Entry count        |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       <URL Entry 1>          ...       <URL Entry N>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : receivs a reply from a service request
 params : 
 returns: SLP_OK if valid
******************************************************************************/
SLPError slp_receive_srvrply(unsigned long ip, SLPHeader *header, char *data)
{
  // Unpack error code, if OK, unpack URLs and call callback, for each URL?
  // In non-DA mode, these may arrive in multiple packets (multicast request).
  // Do we need to check to see if we already have this.
  // Only do this if a) we have a call back b) we have a matching XID
  // This is not thread aware so our sending routine sets the XID
  // and we clear it here.  Sending will fail if XID is already set.

  uint16_t error_code;
  uint16_t url_count;
  SLPUrlEntry urlentry;
  char  *url;

  SLP_UNUSED_ARG(ip);

  // error, did not ask for a reply!
  if (!srvrqst_xid)
    return(SLP_UNEXPECTED_MSG);

  // error, xid does not match, this may not be an error so return OK
  if (srvrqst_xid != header->xid) {
    srvrqst_xid = 0;
    return (SLP_OK);
  }
  
  data = unpackUINT16(data, &error_code);
  if (error_code) {
    // callback with error code
    if (srvrqst_callback) {
      srvrqst_callback(error_code, NULL);
    }
  } else {
    // get URL count
    data = unpackUINT16(data, &url_count);
    // TODO, should we have some sanity limit on the number of URLs to parse?
    while (url_count) {
      // callback with URL
      unpackURL_ENTRY(data, &urlentry);
      // rest of data void if we have authentication
      if (urlentry.num_auth_blocks) 
        break;
      // urlentry contains a pointer to string data that is NOT terminated with a null
      // so we convert it. We MUST free this ourselves!
      url = getSLP_STR(&urlentry.url);
      // callback with URL here...
      if (srvrqst_callback) {
        srvrqst_callback(SLP_OK, url);
      }
      SLP_FREE(url);
      url_count--;
    }
  }
  // reset xid
  srvrqst_xid = 0;
  return SLP_OK;
}
#endif //SLP_IS_UA

#if SLP_IS_SA
/*******************************************************************************
  8.3. Service Registration
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         Service Location header (function = SrvReg = 3)       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                          <URL-Entry>                          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | length of service type string |        <service-type>         \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     length of <scope-list>    |         <scope-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of attr-list string   |          <attr-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |# of AttrAuths |(if present) Attribute Authentication Blocks...\
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send a slp registration
 params : ip - ip of device to send registration to
        : fresh - flag to indicate if the fresh flag should be set
 returns: SLP_OK if valid
        : non-zero if not valid
******************************************************************************/
//FUNCTION TESTED
SLPError slp_send_reg(unsigned long ip, bool fresh)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;

//  char        *url;
  
  SLPString   slp_string; 
  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;
  
  acnlog(LOG_DEBUG | LOG_SLP, "slp_send_reg: fresh: %d", fresh);  
  
  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_reg: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVREG;        // register
  if (fresh) {                                      // flags (and reserved)
    slp_header.flags = SLP_FLAG_FRESH;
  } else {
    slp_header.flags = 0;
  }
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = xid++;                           // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);
  
  slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
  slp_urlentry.url.len = srv_url_len;
  slp_urlentry.url.str = srv_url;
  slp_urlentry.num_auth_blocks = 0;
  offset = packSLP_URL_ENTRY(offset, &slp_urlentry);
  
  // service
  slp_string.len = srv_type_len;
  slp_string.str = srv_type;
  offset = packSLP_STR(offset, &slp_string);
    
  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);
  
  // attr_list
  slp_string.len = attr_list_len;
  slp_string.str = attr_list;
  offset = packSLP_STR(offset, &slp_string);

  // auth blocks    
  offset = packUINT8(offset, 0); // no auth blocks
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 

  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //SLP_IS_SA

#if SLP_IS_SA
/*******************************************************************************
  10.6. Service Deregistration

   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         Service Location header (function = SrvDeReg = 4)     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    Length of <scope-list>     |         <scope-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                           URL Entry                           \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |      Length of <tag-list>     |            <tag-list>         \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send a slp de-registration
 params : ip - ip of device to send registration to
 returns: SLP_OK if valid
        : non-zero if not valid
******************************************************************************/
//FUNCTION TESTED
SLPError slp_send_dereg(unsigned long ip)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;
  
  SLPString   slp_string; 
  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;
  
  acnlog(LOG_DEBUG | LOG_SLP , "slp_send_dereg");  
  
  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_dereg: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;
  
  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVDEREG;      // deregister
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = xid++;                           // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);
  
  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  // URL Entry
  slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
  slp_urlentry.url.len = srv_url_len;
  slp_urlentry.url.str = srv_url;
  slp_urlentry.num_auth_blocks = 0;
  offset = packSLP_URL_ENTRY(offset, &slp_urlentry);

  // tag list (no tags)
  offset = packUINT16(offset,0);
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
   
  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //SLP_IS_SA

#if SLP_IS_DA
/*******************************************************************************
  8.4. Service Acknowledgment
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          Service Location header (function = SrvAck = 5)      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          Error Code           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send a received service ack
 params : IP = 32 bit IP address to send to
        : reply_xid - xid to use
        : error_code - error code to send (if any)
 returns: SLP_OK if success
        : non-zero if not success
******************************************************************************/
//FUNCTION NOT TESTED - (for use as DA)
SLPError slp_send_svrack(unsigned long ip, uint16_t reply_xid, uint16_t error_code) 
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;
  
  SLPHeader   slp_header;

  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_svrack: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVACK;        // register
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = reply_xid;                       // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);
  
  //  add error code
  offset = packUINT16(offset, error_code);
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
  
  // send it!
  ipaddr.addr = ip;

  pkt = pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //NEVER


#if SLP_IS_UA
/*******************************************************************************
10.3. Attribute Request
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Service Location header (function = AttrRqst = 6)       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       length of PRList        |        <PRList> String        \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         length of URL         |              URL              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    length of <scope-list>     |      <scope-list> string      \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |  length of <tag-list> string  |       <tag-list> string       \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |   length of <SLP SPI> string  |        <SLP SPI> string       \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send an attribute request
 params : IP = 32 bit IP address to send to
 returns: SLP_OK if success
        : non-zero if not success
******************************************************************************/
SLPError slp_send_attrrqst(unsigned long ip, char *req_url, char *tags,
  void (*callback) (int error, char *attributes))
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;
  
  SLPString   slp_string; 
  SLPHeader   slp_header;

  // if ip = 0, then use first from list
  if (!ip) {
    if (!da_list[0].ip) {
      return(SLP_PARAMETER_BAD);
    } 
    ip = da_list[0].ip;
  }

  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_attrrqst: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;

  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_ATTRRQST;      // register
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = xid++;                           // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);

  // predicate string(null)
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  // URL 
  if (req_url) {
    slp_string.len = strlen(req_url);
  } else {
    slp_string.len = 0;
  }
  slp_string.str = req_url;
  offset = packSLP_STR(offset, &slp_string);

  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  // tag list
  if (tags) {
    slp_string.len = strlen(tags);
  } else {
    slp_string.len = 0;
  }
  slp_string.str = tags;
  offset = packSLP_STR(offset, &slp_string);

  // slp_spi (null)
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 

  attrrqst_callback = callback;
  attrrqst_xid = slp_header.xid;
  
  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &ipaddr, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //SLP_IS_UA

#if SLP_IS_UA
/*******************************************************************************
10.4. Attribute Reply
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |       Service Location header (function = AttrRply = 7)       |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         Error Code            |      length of <attr-list>    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                         <attr-list>                           \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |# of AttrAuths |  Attribute Authentication Block (if present)  \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : process a received attribute reply
 params : 
        : 
 returns: SLP_OK
******************************************************************************/
SLPError slp_receive_attrrply(unsigned long ip, SLPHeader *header, char *data)
{
  // Unpack error code, if OK, unpack URLs and call callback, for each URL?
  // In non-DA mode, these may arrive in multiple packets (multicast request).
  // Do we need to check to see if we already have this.
  // Only do this if a) we have a call back b) we have a matching XID
  // This is not thread aware so our sending routine sets the XID
  // and we clear it here.  Sending will fail if XID is already set.

  uint16_t error_code;
  //uint16_t attr_length;
  char  *attr_list;
  SLPString slp_str;

  SLP_UNUSED_ARG(ip);

  // error, did not ask for a reply!
  if (!attrrqst_xid)
    return(SLP_UNEXPECTED_MSG);

  // error, xid does not match, this may not be an error so return OK
  if (attrrqst_xid != header->xid) {
    attrrqst_xid = 0;
    return (SLP_OK);
  }
  
  data = unpackUINT16(data, &error_code);
  if (error_code) {
    // callback with error code
    if (attrrqst_callback) {
      attrrqst_callback(error_code, NULL);
    }
  } else {
    // get attributes count
    data = unpackSLP_STR(data, &slp_str);
    // TODO: should we get auth block and abort if it exist (we don't support)

    // get string
    attr_list = getSLP_STR(&slp_str);
    // callback
    attrrqst_callback(error_code, attr_list);
    // free memory
    SLP_FREE(attr_list);
  }
  // reset xid
  srvrqst_xid = 0;
  return SLP_OK;
}
#endif //SLP_IS_UA

#if SLP_IS_SA
/*******************************************************************************
  8.4. Service Acknowledgment
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          Service Location header (function = SrvAck = 5)      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          Error Code           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : process a received service ack
 params : header, parsed SLP header
        : data, pointer to start of service acl data block
 returns: SLP_OK
******************************************************************************/
//FUNCTION TESTED
SLPError slp_receive_svrack(unsigned long ip, SLPHeader *header, char *data)
{
  uint16_t error_code;

  SLP_UNUSED_ARG(header);
  
  data = unpackUINT16(data, &error_code);
  if (error_code == SLP_OK) {
    da_service_ack(ip);
  } else {
    da_delete(ip);
  }
  return SLP_OK;
}
#endif //SLP_IS_SA


#if SLP_IS_UA || SLP_IS_SA
/*******************************************************************************
  8.5. Directory Agent Advertisement
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Service Location header (function = DAAdvert = 8)      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |          Error Code           |  DA Stateless Boot Timestamp  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |DA Stateless Boot Time,, contd.|         Length of URL         |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \                              URL                              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <scope-list>    |         <scope-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <attr-list>     |          <attr-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |    Length of <SLP SPI List>   |     <SLP SPI List> String     \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | # Auth Blocks |         Authentication block (if any)         \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : process a received DaAdvertisement message
 params : header, parsed SLP header
        : data, pointer to start of Adadvert data block
 returns: SLP_OK if valid
        : non-zero if not valid
******************************************************************************/
//FUNCTION TESTED
SLPError slp_receive_daadvert(unsigned long ip, SLPHeader *header, char *data)
{
SLPDAAdvert daadvert;
char       *scope_list;

//    3. UA uses multicast/convergence srvrqsts to discover DAs, then uses
//       that set of DAs.  A UA that does not know of any DAs SHOULD retry
//       DA discovery, increasing the waiting interval between subsequent
//       attempts exponentially (doubling the wait interval each time.)
//       The recommended minimum waiting interval is CONFIG_DA_FIND
//       seconds.
//
//    4. A UA with no knowledge of DAs sends requests using multicast
//       convergence to SAs.  SAs unicast replies to UAs according to the
//       multicast convergence algorithm.

// If message is multicast flag is set, the message is unsolicited (xid should be 0 too)
// otherwise, these should be as a result of us sending a SereviceRequest and it
// should have matching xid values

// if error code is non-zero, rest of packet may not exist!

// After discovering a new DA, a SA MUST wait a random time between 0
// and CONFIG_REG_ACTIVE seconds before sending registration.

// If boot time is 0, it is going down and remove from list.
// Conversly only register to DA if bootime > 0

// Only send reqistrations of the scope matches. If there is no scope, then
// we should reply too
        
// if we don't find any or we loose one, should we resend request for DA?
      
//NOTE on OpenSLP library
//  Unsolicited DAadverts have xid != 0;

/*
   SAs MUST accept multicast service requests and unicast service
   requests.  SAs MAY accept other requests (Attribute and Service Type
   Requests).  SAs MUST listen for multicast DA Advertisements.

*/

  SLP_UNUSED_ARG(ip);
      
  data = unpackUINT16(data, &daadvert.error_code);
  if (daadvert.error_code) {
    return (SLPError)daadvert.error_code;
  }

  data = unpackUINT32(data, &daadvert.boot_time);
  data = unpackSLP_STR(data, &daadvert.url);
  data = unpackSLP_STR(data, &daadvert.scope_list);
  data = unpackSLP_STR(data, &daadvert.attr_list);
  
  // we are going to ignore these for now...
  #ifdef SLP_AUTH
  data = unpackSLP_STR(data, &daadvert.slp_sip);
  data = unpackUINT8(data, &x);
  daadvert.num_auth_blocks = x;
  while (x-- > 0) {
    data = unpackUINT16(data, &length);  // descriptor
    data = data + length;
  }
  #endif
  
  // verify scope
  scope_list = getSLP_STR(&daadvert.scope_list);
  if (scope_list) {
    if (strcasecmp(scope_list, acn_scope_str)) {
      SLP_FREE(scope_list);
      return SLP_OK;
    }
    SLP_FREE(scope_list);
  } else {
    return SLP_MEMORY_ALLOC_FAILED;
  }
  
  // add it to our list
  da_add(&daadvert);

  // One should be able to tell a solicited advertisement from a 
  // unsolicited one based on three flags. First, the xid for unsolicited
  // should be 0 and second, the unsolicited should have the multicast
  // flag set and three, the xid of the solicited one should match our 
  // initial request. The OpenSLP library seem to have a bug where it does 
  // do the first two. So, we will use the xid to match (this might be better
  // anyway. (one can also tell by a unicast destination address but stack
  // does not provide this value)

  
  if (dda_timer.discover && header->xid == dda_timer.xid) {
    // Yep, so send a new one right away
    dda_timer.counter = CONFIG_MC_MAX;
    dda_timer.xmits = 1;
    dda_timer.xid = xid;
    #if SLP_IS_UA || SLP_IS_SA
    return slp_discover_da();
    #endif 
  }
  
  return SLP_NOT_IMPLEMENTED;
}
#endif //SLP_IS_UA || SLP_IS_SA

#if SLP_IS_SA
/*******************************************************************************
  8.6. Service Agent Advertisement
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |        Service Location header (function = SAAdvert = 11)     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         Length of URL         |              URL              \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <scope-list>    |         <scope-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <attr-list>     |          <attr-list>          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  | # auth blocks |        authentication block (if any)          \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 desc   : send a slp advertisment
 params : ip - ip of device to send advertiment to
        : xid - XID to use
 returns: SLP_OK if valid
        : non-zero if not valid
******************************************************************************/
//FUNCTION NOT TESTED (should not be needed for ACN)
SLPError slp_send_saadvert(unsigned long ip, uint16_t reply_xid) 
{
  uint32_t     length;
  char        *data;
  char        *offset;
  SLPError     result;
  struct pbuf *pkt;  /* Outgoing packet */
  struct ip_addr ipaddr;

  // we can't send advertizment if we don't have attribute list
  if (!attr_list) {
    return SLP_INTERNAL_SYSTEM_ERROR;
  }
  
  //char        *url;
  SLPString   slp_string; 
  SLPHeader   slp_header;
  //SLPUrlEntry slp_urlentry;

  acnlog(LOG_DEBUG | LOG_SLP , "slp_send_saadvert");

  pkt = pbuf_alloc(PBUF_TRANSPORT, SLP_MTU, PBUF_TYPE);
  if (!pkt) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_saadvert: Memory allocation error");  
    return SLP_MEMORY_ALLOC_FAILED;
  }

  data = pkt->payload;
  
  // copy pointer
  offset = data;

  // add header
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SAADVERT;      // SA Advertisement
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        // Next Ext Offset
  slp_header.xid = reply_xid;                       // XID
  slp_header.lang_tag.len = local_lang_len;         // Language Tag Length
  slp_header.lang_tag.str = (char*)local_lang_str;  // Language Tag String
  offset = packSLP_HEADER(offset, &slp_header);

   // URL  
   slp_string.len = sa_reg_len;
   slp_string.str = (char*)sa_reg_str;
  offset = packSLP_STR(offset, &slp_string);
  
  // scope-list
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

   // attribute list  
  slp_string.len = attr_list_len;
  slp_string.str = attr_list;
  offset = packSLP_STR(offset, &slp_string);

  // auth blocks    
  offset = packUINT8(offset, 0); // no auth blocks
  
  // adjust length data field and put in udp structrure too
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  // length 
  
  // send it!
  ipaddr.addr = ip;

  pbuf_realloc(pkt, length);
  result = udp_sendto(slp_pcb, pkt, &slpmcast, SLP_RESERVED_PORT);
  pbuf_free(pkt);
  return (result);
}
#endif //SLP_IS_SA

//*****************************************************************************
// desc   : callback for SLP
// params : pkt, pointer to etherent packet packet
//        : data, user defined data var from udp_open
// returns: none
/******************************************************************************/
//FUNCTION TESTED
static void slp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
  char        *slp_data;
  SLPHeader   slp_header; 

  SLP_UNUSED_ARG(arg);
  SLP_UNUSED_ARG(pcb);
  SLP_UNUSED_ARG(port);

  // make sure we have enough bytes..
  slp_data = p->payload;
  if (p->tot_len > 4) {               // need at least 4 bytes for header
    if (slp_data[0] == 2) {         // verify version
      //acnlog(LOG_DEBUG | LOG_SLP , ("slp_recv: from %s: ", ntoa(addr->addr) ) );  
      slp_data = unpackSLP_HEADER(slp_data, &slp_header);
      switch (slp_header.function_id) {
        case SLP_FUNCT_SRVRQST:           //Service Request 
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Request");  
          #if SLP_IS_SA
          slp_receive_srvrqst(addr->addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_SRVRPLY:           //Service Reply
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Reply");  
          #if SLP_IS_UA
          slp_receive_srvrply(addr->addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_SRVREG:            //Service Registration (DA only)
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Register");  
          // No Support Planned
          break;
        case SLP_FUNCT_SRVDEREG:          //Service Deregister   (DA only)
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Deregister");  
          // No Support Planned
          break;
        case SLP_FUNCT_SRVACK:            //Service Acknowledge 
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Ack");  
          #if SLP_IS_SA
          slp_receive_svrack(addr->addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_ATTRRQST:          //Attribute Request    
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Attr Request");  
          // TODO: Add support for this
          break;
        case SLP_FUNCT_ATTRRPLY:          //Attribute Reply
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Attr Reply");  
          #if SLP_IS_UA
          slp_receive_attrrply(addr->addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_DAADVERT:          //DA Advertisement
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: DA Advertisment: %d", slp_header.xid);  
          #if SLP_IS_UA || SLP_IS_SA
          slp_receive_daadvert(addr->addr, &slp_header, slp_data);
          #endif 
          break;
        case SLP_FUNCT_SRVTYPERQST:       //Service Type Request
          acnlog(LOG_DEBUG | LOG_SLP , "%slp_recv: Service Type Request");  
          // No Support Planned
          break;
        case SLP_FUNCT_SRVTYPERPLY:       //Service Type Reply
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Type Reply");  
          // No Support Planned
          break;
        case SLP_FUNCT_SAADVERT:          //SA Advertisement
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: SA Advertisement");  
          // TODO: Support?
          break;
        default:
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Unknown function");  
      } // switch
    } else { // version 2 only
      acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: unsupported version");  
    }
  } // enough data
  
  pbuf_free(p);
}


#if SLP_IS_UA || SLP_IS_SA
/*******************************************************************************

*******************************************************************************/
//FUNCTION TESTED
void  slp_active_discovery_start(void) 
{
  uint16_t  tps_delay;
  
  // "After a UA or SA restarts, its initial DA discovery request SHOULD be
  //  delayed for some random time uniformly distributed from 0 to
  //  CONFIG_START_WAIT seconds."
   
  // Get random number from 0 to CONFIG_START_WAIT.
  // Variable delay is a random number. We really don't care what bits, so 
  // we will pick the top ones
  tps_delay = rand();
  while (tps_delay > CONFIG_START_WAIT) {
    tps_delay = tps_delay >> 1;
  }

  acnlog(LOG_DEBUG | LOG_SLP , "slp_active_discovery_start: delay %d",tps_delay);  
  dda_timer.counter = tps_delay;
  dda_timer.xmits = 0;
  dda_timer.xid = 0;
  dda_timer.discover = 1;
}
#endif //SLP_IS_UA || SLP_IS_SA

/*******************************************************************************
// srand() should be called before slp_init
*******************************************************************************/
//FUNCTION TESTED
void slp_init(void)
{
int x;

  acnlog(LOG_DEBUG | LOG_SLP , "slp_init");
  // sequential transaction id
  xid = rand();           
  // clear da_list
  for (x=0;x<MAX_DA;x++) {
    memset(&da_list[x], 0, sizeof(SLPDa_list));
  }
  // clear timer counts
  memset(&dda_timer, 0, sizeof(SLPdda_timer));

  attr_list = NULL;        // attribute list
  attr_list_len = 0;
  
  srv_url = NULL;
  srv_url_len = 0;

  srv_type = NULL;
  srv_type_len = 0;
  
  msticks = 0;
}


/*******************************************************************************
  start up SLP
*******************************************************************************/
//FUNCTION TESTED
SLPError slp_open(void) 
{
//  portTickType xLastWakeTime;

  acnlog(LOG_DEBUG | LOG_SLP , "slp_open");  

  // sorry, we can only open once
//  if (slp_task) {
//    acnlog(LOG_DEBUG | LOG_SLP , "slp_open: already open");  
//    return SLP_HANDLE_IN_USE;
//  }

  // wait for network to be running. DHCP will bring it up after we get a IP address
//  xLastWakeTime = xTaskGetTickCount();
//  while (!netif_is_up(netif_default)) {
//    vTaskDelayUntil( &xLastWakeTime, 1000/portTICK_RATE_MS);
//  }

  // TODO: make this a const
  // set address
  IP4_ADDR(&slpmcast, 239,255,255,253);

  // add SLP multicast
  igmp_joingroup(&netif_default->ip_addr, &slpmcast);

  // Get Protocol Control Block
  slp_pcb = udp_new();
  if (!slp_pcb)
    return SLP_NETWORK_ERROR;

  // BIND:sets local_ip and local_port
  if (!udp_bind(slp_pcb, IP_ADDR_ANY, SLP_RESERVED_PORT) == ERR_OK)
    return SLP_NETWORK_ERROR;

  // UDP Callback
  udp_recv(slp_pcb, slp_recv, 0);

  /* Start our thread */
//  slp_task = sys_thread_new("SLP", slp_thread, netif_default, SLP_THREAD_STACK, SLP_THREAD_PRIO);
//
//  if (slp_task) {
    return SLP_OK;
//  } else {
//    return SLP_NOT_OPEN;
//  }
}

/*******************************************************************************
  Shut down SLP
*******************************************************************************/
void slp_close(void) 
{
  acnlog(LOG_DEBUG | LOG_SLP , "slp_close");  

  // sorry, not open
//  if (!slp_task) {
//    return;
//  }

  // tell our DAs that we are going away (this is blocking)
  #if SLP_IS_SA || SLP_IS_UA
  da_close(); 
  #endif
  
  // remove our multicast address
  igmp_leavegroup(&netif_default->ip_addr, &slpmcast);

  // shut down thread;
//  sys_thread_delete(slp_task);
 
  // remove our callback
  udp_recv(slp_pcb, NULL, 0);

  // free pcb
  udp_remove(slp_pcb);

  // flag that we are done
//  slp_task = NULL;
}

#if SLP_IS_SA
/*******************************************************************************
// Registers a service URL and service attributes with SLP. 
   TODO: current implementation only allows us to register one component
*******************************************************************************/
//FUNCTION TESTED
SLPError slp_reg(char *reg_srv_url, char *reg_srv_type, char *reg_attr_list) 
{
  acnlog(LOG_DEBUG | LOG_SLP , "slp_reg");

   /* can only have one! */
   if (attr_list) {
    return SLP_PARAMETER_BAD;
   }

  // you must open first
   if (!slp_pcb)  {
     return SLP_NOT_OPEN;
   }
  
  // make sure we have a real attribute
  if (!reg_attr_list) {
    return SLP_PARAMETER_BAD;
  }

  // make sure we have a real url
  if (!reg_srv_url) {
    return SLP_PARAMETER_BAD;
  }

  // make sure we have a real url
  if (!reg_srv_type) {
    return SLP_PARAMETER_BAD;
  }
  
  // save pointer to our attribute list
   attr_list = reg_attr_list;
   attr_list_len = strlen(attr_list);

   srv_url = reg_srv_url;
   srv_url_len = strlen(srv_url);

   srv_type = reg_srv_type;
   srv_type_len = strlen(srv_type);
  
   // send registration to all the DAs
   da_reg();
  
  return SLP_OK;
}
#endif //SLP_IS_SA

#if SLP_IS_SA
/*******************************************************************************

// TODO: Add support for more than one registration
*******************************************************************************/
//FUNCTION TESTED
SLPError slp_dereg(void) 
{
  acnlog(LOG_DEBUG | LOG_SLP , "slp_dereg");

  // are we open?
  if (!slp_pcb)  {
    return SLP_NOT_OPEN;
  }
  
  // did we have a valid registration?
  if (!attr_list) {
    return SLP_PARAMETER_BAD;
  }
  
  // deregister all
  da_dereg();
  
  // nuke our attribute list
  attr_list = NULL;
  
  return SLP_OK;
}
#endif //#SLP_IS_SA

// TODO: This thread is now done by the application thread.
///*******************************************************************************
//  Thread for SLP
//*******************************************************************************/
//void slp_thread( void *pvParameters )
//{
//  portTickType xLastWakeTime;
//
//  /* Parameters are not used - suppress compiler error. */
//  SLP_UNUSED_ARG(pvParameters);
//
//  // start a discovery
//  #if SLP_IS_UA || SLP_IS_SA
//  slp_active_discovery_start();
//  #endif
//
//  /* Initialise xLastWakeTime */
//  xLastWakeTime = xTaskGetTickCount();
//  while (1)   {
//    // call our timer routine
//    slp_tick(0);
//    // now wait
//    vTaskDelayUntil( &xLastWakeTime, 1/portTICK_RATE_MS );
//  }
//}
