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
#tabs=2s
*/
/************************************************************************/
/*
  Description:
  This module creates a SLP inteface. While much of it is generic, there
  are parts that have been hard coded to work in a ACN environment

  Author:
  Bill Florac - Electroinc Theatre Controls, Inc

  Current implementation:
  Both Service Agent (SA) and User Agent (UA) are supported. Directory
  Agent (DA) is not supported. SA and UA modes can be used at the same
  time.

  Limitations:
  SA mode does not support operation without a DA available (at this
  time). Parts of  the code needed for this are in place but they have
  not been completed nor tested.
  
  Notes
  Do NOT use string functions on SLPString structures, they do NOT
  contain a terminating NULL! use getSLP_STR on it first to malloc and
  copy to a new string.

  SLP requires a slp_tick() to be called at 100 ms intervals.

  The treading model uses ACN_PORT_PROTECT() and ACN_PORT_UNPROTECT().
  This simple model prevent two thread from using the same memory
  space.  Basically, the slp_tick() blocks for it's entire loop.
  Likewies, slp_recv() does the same. All functions that can be  called
  externally also block until they are complete. The exception to this
  is da_close() which is only called by slp_close. It allows other
  thread to run while it times out and closes.

  TODO: (see list here)
  - If IP changes, we will need to re-register everthing
  - What to do with authentication. Ignore, discard packet? SLP_AUTH
  - Get rid of malloc and make statics?
  - Older versions of DA's using OpenSLP would send out Unsolicited
    DAadverts with xid != 0. This has been fixed so we can remove our
    special handling.
  - Perhaps msg_ should be done with MALLOC

  Versions:
  0.0.1   Initial release to test integration
  0.0.2   11/2/2008 WRF Rewrite to be platform independent and integrate
          with OpenACN code
*/

/* Library includes*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* General ACN includes */
#include "opt.h"
#if CONFIG_SLP
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"



/* Platform dependent network interface*/
#include "netxface.h"
#include "ntoa.h"
#include "aton.h"

/* Other includes */
#include "netsock.h"  /* network socket storage */
#include "netsock.h"
#include "netxface.h"
#include "cid.h"     /* manage uuids */
#include "pack.h"     /* to pack and unpack data strucures */
#include "msleep.h"    /* platform dependent delay */

/* SLP include */
#include "slp.h"      /* This module */

/* handy macros */
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_SLP, "%s :...", __func__)

/*=========================================================================*/
/* defines */
/*=========================================================================*/
/* maximim number of directory agents to track */
#define MAX_DA      10
/* allows porting */
#define SLP_MALLOC(x)  malloc(x)
#define SLP_FREE(x)    free(x)


/* for LWIP only */
/* Just a quick way to test memory allocation types */
#define PBUF_TYPE PBUF_POOL


/*
Our message buffer uses the same three parameters for different messages
types so these make the code a bit more readable
*/
#define MSG_SRV_TYPE(Msg)  (Msg->param1)
#define MSG_SRV_URL(Msg)   (Msg->param2)
#define MSG_ATTR_LIST(Msg) (Msg->param3)

/* deregistration */
#define MSG_SRV_URL(Msg)   (Msg->param2)

/* service request */
#define MSG_SRV_TYPE(Msg)  (Msg->param1)
#define MSG_PREDICATE(Msg) (Msg->param2)

/*attribute request */
#define MSG_REQ_URL(Msg)   (Msg->param1) 
#define MSG_TAGS(Msg)      (Msg->param2) 

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
uint16_t      xid;              /* sequential transaction id */
SLPDa_list    da_list[MAX_DA];  /* note, this is fixed count! */
SLPdda_timer  dda_timer;        /* timer to manage finding Directory Agents */

ip4addr_t     slpmcast;         /* multicast address for SLP */
netxsocket_t *slp_socket = NULL;/* socket for slp to use */
unsigned int  msticks;          /* tick counter (used for some debugging) */

#if SLP_IS_UA || SLP_IS_SA
/* TODO: this could/should be also be a malloc() version with linked list */
#define MAX_REG_PROPS  10
SLPMsg  Msgs[MAX_RQST];               /* for tracking sending, resending returned ack of messsages */
SLPRegProp reg_props[MAX_REG_PROPS];  /*registration properties for each component */
#endif

/*=========================================================================*/
/* local function declarations */
/*=========================================================================*/
/* SA functions */
#if SLP_IS_SA
SLPError  da_reg(SLPDa_list *da);
void      da_reg_all(void);
void      da_dereg_all(void);
SLPError  da_reg_prop(SLPDa_list *da, SLPRegProp *reg_prop, int delay);
#endif

/* UA and SA functions */
#if SLP_IS_SA || SLP_IS_UA
void        da_ip_list(char *str);
SLPError    da_add(SLPDAAdvert *daadvert);
__inline    void da_delete(SLPDa_list *da);
SLPError    da_delete_by_ip(ip4addr_t ip);
void        da_delete_all(void);
int         da_count(void);
void        da_close(void);
SLPError    slp_receive_daadvert(ip4addr_t ip, SLPHeader *header, char *data);
SLPError    slp_discover_da(void);
SLPDa_list *da_first(void);
SLPDa_list *da_find_by_ip(ip4addr_t ip);
SLPError    da_service_ack(SLPDa_list *da);
#endif

/* more packing/unpacking */
char *unpackSLP_STR(char *data, SLPString *slp_str);
char *packSLP_STR(char *data, SLPString *slp_str);
char *packSLP_HEADER(char* data, SLPHeader *header);
char *unpackSLP_HEADER(char* data, SLPHeader *header);
char *getSLP_STR(SLPString *slp_str);
char *packSLP_URL_ENTRY(char* data, SLPUrlEntry *url_entry);
char *unpackURL_ENTRY(char* data, SLPUrlEntry *urlentry);

/* UA only functions (also see header file) */
#if SLP_IS_UA
SLPError slp_send_srvrqst(SLPMsg *Msg);
SLPError slp_receive_srvrply(ip4addr_t ip, SLPHeader *header, char *data);
SLPError slp_send_attrrqst(SLPMsg *Msg);
/*  void (*callback) (int error, char *attributes)); */
SLPError slp_receive_attrrply(ip4addr_t ip, SLPHeader *header, char *data);
#endif

/* SA only functions */
#if SLP_IS_SA
SLPError slp_send_saadvert(ip4addr_t ip, uint16_t reply_xid);
SLPError slp_send_reg(SLPMsg *Msg, bool fresh);
SLPError slp_send_dereg(SLPMsg *Msg);
SLPError slp_receive_srvrqst(ip4addr_t ip, SLPHeader *header, char *data);
SLPError slp_send_srvrply(ip4addr_t ip, uint16_t reply_xid, uint16_t error_code);
SLPError slp_receive_svrack(ip4addr_t ip, SLPHeader *header, char *data);
#endif

/* DA only functions (not supported) */
#if SLP_IS_DA
SLPError  slp_send_svrack(ip4addr_t ip, uint16_t reply_xid, uint16_t error_code);
#endif

/****************************************************************/
/* TODO: move this to utility */
/* This function compares the two strings, disregarding case. */
/* note: GCC has a builtin function of the same name that can be access via __builtin_strcasecmp() */
static int __strcasecmp (const char *s1, const char *s2)
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

/******************************************************************************
 The following msg_ functions manage a static memory pool of message buffer
 These are used to track a messages and it's response and to provide data for 
 resending a message
 ******************************************************************************/

/* static pointer to first in linked list */
SLPMsg *FirstMsg = NULL;

/******************************************************************************/
/* Find fisrt message in our pool */
__inline SLPMsg *msg_first(void){
  return FirstMsg;
}

/******************************************************************************/
/* Create a new message */
SLPMsg *msg_new(uint16_t xid){
  SLPMsg *Msg = Msgs;

  while (Msg < Msgs + MAX_RQST) {
    if (Msg->xid == 0) {
      Msg->counter = 0;
      Msg->xid = xid;
      /* now put the new one at the start of the link */
      Msg->next = FirstMsg?FirstMsg:NULL;
      FirstMsg = Msg;
      return(Msg);
    }
    Msg++;
  }
  /* No free one found, return NULL */
  return NULL;
}

/******************************************************************************/
/* Free a message */
SLPMsg *msg_free(SLPMsg *Msg)
{
  SLPMsg *aMsg;

  /* if at top just nuke it */
  if (Msg == FirstMsg) {
    Msg->xid = 0;
    /* And make the next message the first one */
    FirstMsg = Msg->next;
    return FirstMsg;
  }
  
  /* otherwise we need to point around it */
  aMsg = FirstMsg;
  while (aMsg) {
    if (aMsg->next == Msg) {
      /* ok, the next one is the one we are looking for! */
      aMsg->next = Msg->next;
      Msg->xid = 0;
      return Msg->next;
    }
    aMsg = aMsg->next;
  }
  /* Not found, return NULL */
  return NULL;
}

/******************************************************************************/
/* find a mesage based on it's xid */
SLPMsg *msg_find_by_xid(uint16_t xid){
  SLPMsg *Msg;

  Msg = msg_first();
  while (Msg) {
    if (Msg->xid == xid) {
      return(Msg);
    }
    Msg = Msg->next;
  }
  /* Not found, return NULL */
  return NULL;
}

/******************************************************************************/
/* Count the number of valid messages */
int msg_count(void)
{
  SLPMsg *Msg;
  int     cnt = 0;

  Msg = msg_first();
  while (Msg) {
    cnt++;
    Msg = Msg->next;
  }
  return cnt;
}

/******************************************************************************
 The following msg_ functions manage a static memory pool of registration properties.
 These are used to allow auto-registration to new DA's as they become available.
 ******************************************************************************/

/******************************************************************************/
/* add a new registration property */
SLPRegProp *reg_prop_add(char *reg_srv_url, char *reg_srv_type, char *reg_attr_list)
{
  SLPRegProp *p = reg_props;

  /* while we have three properties, we only use the first on as our key and empty flag */
  while (p < reg_props + MAX_REG_PROPS) {
    if (p->reg_srv_url == NULL) {
      p->reg_srv_url = reg_srv_url;
      p->reg_srv_type = reg_srv_type;
      p->reg_attr_list = reg_attr_list;
      return p;
    }
    p++;
  }
  /* No free one found, return NULL */
  return NULL;
}

/******************************************************************************/
/* delete a registration property */
SLPError reg_prop_del(SLPRegProp *reg_prop) 
{
  reg_prop->reg_srv_url = NULL;
  reg_prop->reg_srv_type = NULL;
  reg_prop->reg_attr_list = NULL;
  return SLP_OK;
}

/* find a registration property byt it's URL */
SLPRegProp *reg_prop_find(char *reg_srv_url) 
{
  SLPRegProp *p = reg_props;

  /* got to have a real string (even if empty) */
  assert(reg_srv_url);

  while (p < reg_props + MAX_REG_PROPS) {
    if (p->reg_srv_url == reg_srv_url) {
      return p;
    }
    p++;
  }
  return NULL;
}


/******************************************************************************/
/* Count registration properties */
int reg_prop_count(void) 
{
  SLPRegProp *p = reg_props;

  int result = 0;

  while (p < reg_props + MAX_REG_PROPS) {
    if (p->reg_srv_url) {
      result++;
    }
    p++;
  }
  return result;
}

/*============================================================================*/
/* Some routines to manage our DA (directory agent) list
 * We manage a limited amount of Directory agents
 *============================================================================*/

/******************************************************************************/
/* desc   : return a string listing all our DAs
 * params : str - pointer to start of string
 * returns: void
 */
#if SLP_IS_SA || SLP_IS_UA
void da_ip_list(char *str)
{
  int         first = SLP_TRUE;
  SLPDa_list *da = da_list;

  str[0] = 0;
  while (da < da_list + MAX_DA) {
    if (da->ip) {
      if (first) {
        SPRINTF(str, "%s", ntoa(da->ip));
      } else {
        SPRINTF(str, "%s, %s", str, ntoa(da->ip));
      }
      first = SLP_FALSE;
    }
    da++;
  }
}
#endif /* SLP_IS_SA || SLP_IS_UA */


#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : clears DA list
 * params : none
 * returns: void
 * note   : does not do deregister them
 */
void da_delete_all(void)
{
  SLPDa_list *da = da_list;

  while (da < da_list + MAX_DA) {
    da_delete(da);
    da++;
  }
}
#endif /*SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : close and delete our DAs
 * params : none
 * returns: void
 */
/* Note: this is the only internal routine where ACN_PROTECT() is called  */
void da_close(void)
{
  int           ticks = 0;
  acn_protect_t protect;

  #if SLP_IS_SA

  slp_dereg_all();

  /* wait for DAs to go to close state*/
  while (1) {
    /* look at open messages here */
    protect = ACN_PORT_PROTECT();
    if (!msg_count()) {
      ACN_PORT_UNPROTECT(protect);
      break;
    }
    ACN_PORT_UNPROTECT(protect);
    msleep(100);
    ticks++;
    if (ticks > 20) {
      /* Just give up */
     acnlog(LOG_DEBUG | LOG_SLP , "slp da_close: giving up on closing DA's");
     break;
    }
  }

  /* now nuke them */
  da_delete_all();
  #endif
  return;
}
#endif /* SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : adds DA to our list
 * params : daadvert - advertisement structure
 * returns: SLP_OK if OK, or SLP_DA_LIST_FULL list is full
 */
SLPError da_add(SLPDAAdvert *daadvert)
{
  char         *ip_str;
  ip4addr_t     ip;
  SLPDa_list   *da;

  /* Get ip */
  ip_str = getSLP_STR(&daadvert->url);
  if (ip_str) {
    /* convert string to ip (string MUST be: service:directory-agent://<ip>) */
    ip = aton((char *)&ip_str[da_req_len + 3]);
    SLP_FREE(ip_str);
  } else {
    return SLP_FAIL;
  }

  da = da_find_by_ip(ip) ;
  if (da) {
    if (daadvert->boot_time == 0) {
      /* da is going down, just nuke it */
      da_delete(da);
      return SLP_OK;
    }
    /* see if has rebooted */
    if (daadvert->boot_time == da->boot_time) {
      /* already have this one, do nothing...well...ok, mark it as still active. */
      da->timeout = CONFIG_DA_BEAT;
      return SLP_OK;
    } else {
      /* must of rebooted, nukeit to force a registration */
      da_delete(da);
      return SLP_OK;
    }
  } else {
    /* trick to find first empty */
    da = da_find_by_ip(0);
    if (da) {
      da->ip = ip;              /* flags it as used */
      da->timeout = CONFIG_DA_BEAT;
      da->boot_time = daadvert->boot_time;
      acnlog(LOG_DEBUG | LOG_SLP , "slp da_add: added DA: %s", ntoa(ip));
      /* if we have an attribute list, then set it to register */
      #if SLP_IS_SA
      /* put message in our stack */
      da_reg(da);
      #endif
    } else {
      return SLP_DA_LIST_FULL;
    }
  }
  return SLP_OK;
}
#endif /* SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : remove a DA from our list by it's IP
 * params : ip - 32 bit IP address
 * returns: SLP_OK if OK, or SLP_DA_NOT_FOUND if not found
 */
SLPError da_delete_by_ip(ip4addr_t ip)
{
 SLPDa_list *da = da_list;

  while (da < da_list + MAX_DA) {
    if(da->ip == ip) {
      da_delete(da);
      return SLP_OK;
    }
    da++;
  }
  return SLP_DA_NOT_FOUND;
}

/******************************************************************************/
/* desc   : remove a DA from our list
 * params : da - DA to remove
 * returns: 
 */
__inline void da_delete(SLPDa_list *da)
{
  assert(da);
  da->ip = 0;
}
#endif /* SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : count DAs
 * params : none
 * returns: number of DAs found
 */
int da_count(void)
{
  int result = 0;
  SLPDa_list *d = da_list;

  while (d < da_list + MAX_DA) {
    if (d->ip) {
      result ++;
    }
    d++;
  }
  return result;
}
#endif /* SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : find first da in our list
 * params : none
 * returns: Pointer to our DA
 */
SLPDa_list *da_first(void)
{
  SLPDa_list *d = da_list;
  while (d < da_list + MAX_DA) {
    if (d->ip) {
      return d;
    }
    d++;
  }
  return NULL;
}
#endif /* #if SLP_IS_SA || SLP_IS_UA */

#if SLP_IS_SA || SLP_IS_UA
/******************************************************************************/
/* desc   : find da in our list by it's IP
 * params : IP - IP of DA to find
 * returns: pointer to our DA
 */
SLPDa_list *da_find_by_ip(ip4addr_t ip)
{
  SLPDa_list *da = da_list;
  while (da < da_list + MAX_DA) {
    if (da->ip == ip) {
      return da;
    }
    da++;
  }
  return NULL;
}
#endif /* #if SLP_IS_SA || SLP_IS_UA */


#if SLP_IS_SA
/******************************************************************************/
/* desc   : process registration/deregistrion ack from a directory agent
 * params : ip - 32 bit ip address to ack to
 * returns: SLP_OK if OK, or SLP_DA_NOT_FOUND if address was not in our list
 */
SLPError da_service_ack(SLPDa_list *da)
{
  assert(da);

  /* update timer */
  da->timeout = CONFIG_DA_BEAT;
  return SLP_OK;
}
#endif /* SLP_IS_SA */


#if SLP_IS_SA
/******************************************************************************/
/* desc   : tell a DA about our registration
 * params : idx - index to DA list
 * returns: void
 * note   : does not verify idx nor if there is an ip in list
 */
SLPError da_reg_prop(SLPDa_list *da, SLPRegProp *reg_prop, int delay)
{
  SLPMsg       *Msg;

  Msg = msg_new(xid++);
  if (Msg) {
    Msg->da = da;
    Msg->type = mtREG;
    Msg->counter = delay;
    Msg->retries = SLP_RETRIES;

    /* Yuck! copy strings */
    MSG_SRV_TYPE(Msg) = SLP_MALLOC(strlen(reg_prop->reg_srv_type+1));
    if (!MSG_SRV_TYPE(Msg)) {
      msg_free(Msg);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_SRV_TYPE(Msg),reg_prop->reg_srv_type);


    MSG_ATTR_LIST(Msg) = SLP_MALLOC(strlen(reg_prop->reg_attr_list)+1);
    if (!MSG_ATTR_LIST(Msg)) {
      SLP_FREE(MSG_SRV_TYPE(Msg));
      msg_free(Msg);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_ATTR_LIST(Msg),reg_prop->reg_attr_list);

    MSG_SRV_URL(Msg) = SLP_MALLOC(strlen(reg_prop->reg_srv_url)+1);
    if (!MSG_SRV_URL(Msg)) {
      SLP_FREE(MSG_SRV_TYPE(Msg));
      SLP_FREE(MSG_ATTR_LIST(Msg));
      msg_free(Msg);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_SRV_URL(Msg),reg_prop->reg_srv_url);

    /* TODO: add call back to this */
    Msg->callback = NULL;
  } else {
    return SLP_MEMORY_ALLOC_FAILED;
  }
  return SLP_OK;
}
#endif /* SLP_IS_SA */


#if SLP_IS_SA
/******************************************************************************/
SLPError da_reg(SLPDa_list *da)
{
  uint16_t      tps_delay;
  SLPRegProp   *reg_prop;

  assert(da);

  /* get a random delay */
  tps_delay = rand(); /* returns int in range 0 to RAND_MAX (at least 32k) */
  /* scale the random number to max 3 seconds in units of system timer ticks */
  while (tps_delay > CONFIG_REG_ACTIVE ) {
    tps_delay = tps_delay >> 1;
  }

  /* command the registration to the slp_tick() routine */
  reg_prop = reg_props;
  while (reg_prop < reg_props + MAX_REG_PROPS) {
    if (reg_prop->reg_srv_url) {
      if (da_reg_prop(da, reg_prop, tps_delay) == SLP_FAIL) {
        return SLP_FAIL;
      }
    }
    reg_prop++;
  }
  /* tag it as updated */
  da->counter = SLP_REGIGISTION_REFRESH;
  return SLP_OK;
}
#endif /* SLP_IS_SA */

#if SLP_IS_SA
/******************************************************************************/
/* desc   : tell all DAs about our registrations
 * params : none
 * returns: void
 */
void da_reg_all(void)
{
  SLPDa_list *da = da_list;

  while (da < da_list + MAX_DA) {
    /* if this DA has an IP */
    if(da->ip) {
      /* register with this DA */
      da_reg(da);  /* tell a DA about our registration */
    }
    da++;
  }
}
#endif /* SLP_IS_SA */

#if SLP_IS_SA
/******************************************************************************/
/* desc   : tell DA's about our deregistrations
 * params : none
 * returns: void
 */
SLPError da_dereg_prop(SLPDa_list *da, SLPRegProp   *reg_prop)
{
  SLPMsg       *Msg;

  Msg = msg_new(xid++);
  if (Msg) {
    Msg->da = da;
    Msg->type = mtDEREG;
    Msg->counter = 0;
    Msg->retries = SLP_RETRIES;

    /* Yuck! copy strings */
    MSG_SRV_URL(Msg) = SLP_MALLOC(strlen(reg_prop->reg_srv_url)+1);
    if (!MSG_SRV_URL(Msg)) {
      msg_free(Msg);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_SRV_URL(Msg), reg_prop->reg_srv_url);

    /* TODO: add call back to this */
    Msg->callback = NULL;
  } else {
    return SLP_MEMORY_ALLOC_FAILED;
  }
  return SLP_OK;
}
#endif /* SLP_IS_SA */


#if SLP_IS_SA
/******************************************************************************/
void da_dereg_all(void)
{
  SLPDa_list *da = da_list;
  SLPRegProp *reg_prop = reg_props;

  while (reg_prop < reg_props + MAX_REG_PROPS) {
    if (reg_prop->reg_srv_url) {
      while (da < da_list + MAX_DA) {
        if (da->ip) {
          da_dereg_prop(da, reg_prop);
        }
        da++;
      }
      reg_prop_del(reg_prop);
    }
    reg_prop++;
  }
}

#endif /* SLP_SA */



/*============================================================================*/
/* Some SLP specific packing functions
 * These should be processor independent and use the processor dependent
 * pack.c file to do the actual parsing
 */
/*============================================================================*/

/*******************************************************************************
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Length of <string>        |           <string>            \
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*******************************************************************************/
/* desc   : unpack a SLP string containing a length and string data
 * params : data, pointer to start of string
 *        : slp_str, structure to write
 * returns: pointer past end of string
 *
 * NOTE: The string data is pointed into the buffer and NOT copied!
 *       this avoids having to alloc but one can NOT use str functions
 *       as there is no termination null
 */
/******************************************************************************/
char *unpackSLP_STR(char *data, SLPString *slp_str)
{
  /* pull out the two byte length field */
  data = unpackUINT16(data, &slp_str->len);
  /* if there is data present */
  if (slp_str->len > 0) {
    slp_str->str = data;  /* save pointer to the data */
    data += slp_str->len; /* point to the next length field */
  }
  return (data); /* pointer to next length field */
}

/******************************************************************************/
/* desc   : Unpack a SLPString to a real string.
 * params : slp_str - pointer to SLPString structure
 * note   : YOU MUST FREE THIS YOURSELF WITH SLP_FREE()
 */
/******************************************************************************/
char *getSLP_STR(SLPString *slp_str)
{
  char     *result;
  uint16_t  len;

  /* get length */
  len = slp_str->len;
  /* small check on length */
  if (len > 2000) /* a guess size TODO": This should be based on interface MTU */
    len = 0;
  result = (char*)SLP_MALLOC(len+1);
  if (result) {
    /* copy to a real string */
    memcpy(result, slp_str->str, len);
    /* add terminating null */
    result[len] = 0;
  };
  return result; /* pointer to new real string */
}

/******************************************************************************/
/* desc   : pack a SLP string containing a length and string data
 * params : data, pointer to start of string
 *        : slp_str, structure to read
 * returns: pointer past end of string
 *        : string structure gets put into data
 */
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
/* desc   : pack a SLP header
 * params : data, points to start of header
 *          header, structure to write
 * returns: pointer past end of header
 *          header stucture get put into data
 */
/******************************************************************************/
char *packSLP_HEADER(char* data, SLPHeader *header)
{
  data = packUINT8(data, header->version);       /* version */
  data = packUINT8(data, header->function_id);   /* function-id */
  data = packUINT24(data, header->length);       /* length */
  data = packUINT16(data, header->flags);        /* flags */
  data = packUINT24(data, header->ext_offset);   /* Next Ext Offset */
  data = packUINT16(data, header->xid);          /* XID */
  data = packSLP_STR(data, &header->lang_tag);   /* Language Tag Length */
  return (data);
}

/******************************************************************************/
/* desc   : unpack a SLP header
 * params : data, points to start of header
 *          header, header structure to place data
 * returns: pointer past end of header
 *        : modifies header structure
 */
/******************************************************************************/
char *unpackSLP_HEADER(char* data, SLPHeader *header)
{
  data = unpackUINT8(data, &header->version);       /* version */
  data = unpackUINT8(data, &header->function_id);   /* function-id */
  data = unpackUINT24(data, &header->length);       /* length */
  data = unpackUINT16(data, &header->flags);        /* flags */
  data = unpackUINT24(data, &header->ext_offset);   /* Next Ext Offset */
  data = unpackUINT16(data, &header->xid);          /* XID */
  data = unpackSLP_STR(data, &header->lang_tag);    /* Language Tag */
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
/* desc   : unpack a SLP header
 * params : data, points to start of header
 *          header, header structure to place data
 * returns: pointer past end of header
 *        : modifies header structure
 */
/******************************************************************************/
char *packSLP_URL_ENTRY(char* data, SLPUrlEntry *urlentry)
{
  /* url-entry */
  data = packUINT8(data, 0);                        /* Reserved */
  data = packUINT16(data, urlentry->lifetime);      /* Lifetime */
  data = packSLP_STR(data, &urlentry->url);         /* url */
  data = packUINT8(data,0);                         /* # of URL Auths (not supported) */
  return (data);
}

/* return URL from data */
/* note: if number of authentication blocks is not NULL, the pointer data, will be void */
/* and the rest of the packet discarded. */
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
/* desc   : unpack a SLP url entry
 * params : data, points to start of url entry
 *          urlentry, urlentry structure to place data
 * returns: pointer past end of urlentry
 *        : modifies urlentry structure
 * notes  : if number of authentication blocks is not NULL, the pointer data, will be void
 *          and the rest of the packet discarded
 */
/******************************************************************************/
char *unpackURL_ENTRY(char* data, SLPUrlEntry *urlentry)
{
  uint8_t dummy;

  data = unpackUINT8(data, &dummy);  /* reserved, toss this out */
  data = unpackUINT16(data, &urlentry->lifetime);
  data = unpackSLP_STR(data, &urlentry->url);
  data = unpackUINT8(data, &urlentry->num_auth_blocks);
  return (data);
}

/******************************************************************************/
/* Check and process DA timers */
/* called once per SLP_TMR_INTERVAL */
void slp_tick(void *arg)
{
  acn_protect_t  protect;
  SLPMsg         *Msg;
  SLPDa_list     *da = da_list;

  UNUSED_ARG(arg);

  /* count milliseconds */
  msticks += SLP_TMR_INTERVAL;

  protect = ACN_PORT_PROTECT();

#if SLP_IS_SA || SLP_IS_UA
  /* see if we need to send DA discovers */
  if (dda_timer.discover) {
    if (dda_timer.counter == 0) {
      /* turn it off if we have sent max. */
      /* SLP spec says that we should keep sending untill PRLISt does not change so this is a slight deviation */
      if (dda_timer.xmits == CONFIG_MC_RETRY_COUNT) {
        /* set it up for next hit */
        dda_timer.counter = CONFIG_DA_FIND; /* time between sending Active Discovery messages. (default: 15 minutes) */
        dda_timer.xmits = 0;
        dda_timer.xid = 0;
      } else {
        dda_timer.counter = CONFIG_MC_MAX;  /* retry (default: 15 seconds) */
        dda_timer.xmits++;
        dda_timer.xid = xid;
        #if SLP_IS_UA || SLP_IS_SA
        slp_discover_da(); /* send a service request for da discovery */
        #endif
        /* only send once per loop */
        ACN_PORT_UNPROTECT(protect);
        return;
      }
    } else {
      dda_timer.counter--;
    }
  }
  /* loop for all DA's */
  while (da < da_list + MAX_DA) {
    /* if this DA has a valid IP address */
    if (da->ip) {
      /* test to see if DA has gone dead */
      da->timeout--;
      /* if DA timed out */
      if (!da->timeout) {
        /* delete this DA */
        acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: Removing non-responsive DA ");
        da_delete(da);
        continue;
      }
      /* test to see fi we need to resend our registration to this DA */
      da->counter--;
      if (!da->counter) {
        da_reg(da);
      }
    }
    da++;
  }

  Msg = msg_first();
  while (Msg) {
    if (Msg->xid && Msg->counter == 0) {
      switch (Msg->type) {
        case mtREG:
          if (Msg->retries) {
            slp_send_reg(Msg, SLP_TRUE);
            Msg->counter = CONFIG_RETRY;
            Msg->retries--;
          } else {
            /* give up trying to send and nuke DA*/
            acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: registration failed");
            SLP_FREE(MSG_SRV_TYPE(Msg));
            SLP_FREE(MSG_SRV_URL(Msg));
            SLP_FREE(MSG_ATTR_LIST(Msg));
            da_delete(Msg->da);
            Msg = msg_free(Msg);
          }
          break;
        case mtDEREG:
          if (Msg->retries) {
            slp_send_dereg(Msg);
            Msg->counter = CONFIG_RETRY;
            Msg->retries--;
          } else {
            /* give up trying to send and nuke DA*/
            acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: deregistration failed");
            SLP_FREE(MSG_SRV_URL(Msg));
            da_delete(Msg->da);
            Msg = msg_free(Msg);
          }
          break;
        case mtSRVRQST:
          if (Msg->retries) {
            slp_send_srvrqst(Msg);
            Msg->counter = CONFIG_RETRY;
            Msg->retries--;
          } else {
            /* give up trying to send and nuke DA*/
            acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: service request failed");
            SLP_FREE(MSG_SRV_TYPE(Msg));
            SLP_FREE(MSG_PREDICATE(Msg));
            da_delete(Msg->da);
            Msg = msg_free(Msg);
          }
          break;
        case mtATTRRQST:
          if (Msg->retries) {
            slp_send_attrrqst(Msg);
            Msg->counter = CONFIG_RETRY;
            Msg->retries--;
          } else {
            /* give up trying to send and nuke DA*/
            acnlog(LOG_DEBUG | LOG_SLP , "slp_tick: attribute request failed");
            SLP_FREE(MSG_REQ_URL(Msg));
            SLP_FREE(MSG_TAGS(Msg));

            da_delete(Msg->da);
            Msg = msg_free(Msg);
          }
          break;
        default:
          /* no clue, just nuke it */
         Msg = msg_free(Msg);
      }
    } else {
      Msg->counter--;
      Msg = Msg->next;
    }
  }
#endif /* SLP_IS_SA || SLP_IS_US */

  ACN_PORT_UNPROTECT(protect);
}

#if SLP_IS_UA
SLPError slp_srvrqst(ip4addr_t ip, char *req_srv_type, char *req_predicate,
                     void (*callback) (int error, char *url, int count)){

  SLPMsg       *Msg;
  acn_protect_t protect;
  SLPDa_list   *da;

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_srvrqst: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  protect = ACN_PORT_PROTECT();
  /* if ip = 0, then use first from list */
  if (!ip) {
    da = da_first();
    if (!da) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_DEBUG | LOG_SLP , "slp_srvrqst: No DA available");
      return(SLP_PARAMETER_BAD);
    }
  } else {
    da = da_find_by_ip(ip); 
    if (!da) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_DEBUG | LOG_SLP , "slp_srvrqst: DA not found");
      return(SLP_PARAMETER_BAD);
    }
  }

  /* put message in our stack */
  Msg = msg_new(xid++);
  if (Msg) {
    Msg->da = da;
    Msg->type = mtSRVRQST;
    Msg->counter = 0;
    Msg->retries = SLP_RETRIES;

    /* Yuck! copy strings */
    MSG_SRV_TYPE(Msg) = SLP_MALLOC(strlen(req_srv_type) + 1);
    if (!MSG_SRV_TYPE(Msg)) {
      msg_free(Msg);
      ACN_PORT_UNPROTECT(protect);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_SRV_TYPE(Msg), req_srv_type);

    MSG_PREDICATE(Msg) = SLP_MALLOC(strlen(req_predicate) + 1);
    if (!MSG_PREDICATE(Msg)) {
      msg_free(Msg);
      SLP_FREE(MSG_SRV_TYPE(Msg));
      ACN_PORT_UNPROTECT(protect);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_PREDICATE(Msg), req_predicate);

    Msg->callback = callback;
  } else {
    ACN_PORT_UNPROTECT(protect);
    return SLP_MEMORY_ALLOC_FAILED;
  }
  ACN_PORT_UNPROTECT(protect);
  return SLP_OK;
}

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

/* desc   : sends a service request
 * params : ip - IP to send it to
 * returns: SLP_OK if sent
 *        : error message if fail
 */
/******************************************************************************/
SLPError slp_send_srvrqst(SLPMsg *Msg)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  /* struct ip_addr ipaddr; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPString   slp_string;
  SLPHeader   slp_header;

  LOG_FSTART();


  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);
  if (!data) {
    return SLP_MEMORY_ALLOC_FAILED;
  }

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;           /* version */
  slp_header.function_id = SLP_FUNCT_SRVRQST; /* request */
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                  /* Next Ext Offset */
  slp_header.xid = Msg->xid;                   /* XID */
  slp_header.lang_tag.str = (char*)local_lang_str;
  slp_header.lang_tag.len = local_lang_len;
  offset = packSLP_HEADER(offset, &slp_header);

  /* TODO: If running without DA, */
  /* we need to make a SA list of SA's we have */
  /* This then would also go out multicast */
  /* PRList */
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  /* service-type */
  if (MSG_SRV_TYPE(Msg)) {
    slp_string.len = strlen(MSG_SRV_TYPE(Msg));
  } else {
    slp_string.len = 0;
  }
  slp_string.str = MSG_SRV_TYPE(Msg);
  offset = packSLP_STR(offset, &slp_string);

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  /* predicate string */
  if (MSG_PREDICATE(Msg)) {
    slp_string.len = strlen(MSG_PREDICATE(Msg));
  } else {
    slp_string.len = 0;
  }
  slp_string.str = MSG_PREDICATE(Msg);
  offset = packSLP_STR(offset, &slp_string);

  /* slp_spi (null) */
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = Msg->da->ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif  /* SLP_IS_UA */

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
/* FUNCTION NOT TESTED (for use without DA) */
/* TODO, compare needed to be looked at. */
/* We need to check on partial compare on service type' */
/* we need to compare on predicate string */
/* May have problems with long pr_list....(wrf) */
SLPError slp_receive_srvrqst(ip4addr_t ip, SLPHeader *header, char *data)
{
  SLPSrvRequest srv_req;
  char *our_ip;
  char *pr_list;
  char *scope_list;
  char *service_type;
  SLPMsg *Msg;

  LOG_FSTART();

  /* unpack the remaining 5 fields in the serv req message into the struct srv_req */
  data = unpackSLP_STR(data, &srv_req.pr_list);
  data = unpackSLP_STR(data, &srv_req.service_type);
  data = unpackSLP_STR(data, &srv_req.scope_list);
  data = unpackSLP_STR(data, &srv_req.predicate);
  data = unpackSLP_STR(data, &srv_req.slp_spi);

  if (srv_req.slp_spi.len != 0) {
    slp_send_srvrply(ip, header->xid, SLP_AUTHENTICATION_UNKNOWN);
    return SLP_OK;
  }

  /* convert our IP from ascii to an int */
  our_ip = ntoa(netx_getmyip(0));

  /* copy previous response string to a real string. */
  pr_list = getSLP_STR(&srv_req.pr_list);

  /* if we have a pointer to a previous response string (which may be empty) */
  if (pr_list) {
    /* check to see if we are prlist (else don't reply) */
    /* if our IP is not in the pr list */
    if (!strstr(pr_list, our_ip)) {
      SLP_FREE(pr_list); /* return the string buffer */
      /* copy scope list string to a real string. */
      scope_list = getSLP_STR(&srv_req.scope_list);
      if (scope_list) { /* if we have a string (may be empty) */
        /* check to see if scope is correct or blank */
        if (scope_list[0] == 0 || (!__strcasecmp(scope_list, acn_scope_str))) {
          SLP_FREE(scope_list); /* return the string buffer */
          /* copy service type string to a real string. */
          service_type = getSLP_STR(&srv_req.service_type);
          if (service_type) { /* if we have a string (may be empty) */
            /* check service type */
            /* if service type string = "service:service-agent" */
            if (!__strcasecmp(sa_reg_str, service_type)) {
              SLP_FREE(service_type); /* return the string buffer */
              /* SEND SAAdvert */
              slp_send_saadvert(ip, header->xid);
            } else {
              /* if service type string = "service:acn.esta" */
              /* if (!__strcasecmp("service:acn.esta", service_type)) { */
              Msg = msg_find_by_xid(header->xid);
              if (Msg) {
                if (!__strcasecmp(MSG_SRV_TYPE(Msg), service_type)) {
                  /* yep, that's us... */
                  SLP_FREE(service_type); /* return the string buffer */
                  if (!(header->flags & SLP_FLAG_MCAST)) {
                    slp_send_srvrply(ip, header->xid, SLP_OK);
                  }
                  /* SEND SAAdvert here */
                } else {
                  SLP_FREE(service_type);
                }
              } else {
                /* not our service so just exit out */
                SLP_FREE(service_type);
              }
            }
          } else {
            return SLP_MEMORY_ALLOC_FAILED;
          }
        } else {
          /* wrong scope, if unicast report error */
          SLP_FREE(scope_list);
          if (!(header->flags & SLP_FLAG_MCAST)) {
           slp_send_srvrply(ip, header->xid, SLP_SCOPE_NOT_SUPPORTED);
          }
        }
      } else { /* !(scope_list) */
        return SLP_MEMORY_ALLOC_FAILED;
      }
    } else  { /* strstr(pr_list, our_ip) */
      /* we are in prlist so just exit out */
      SLP_FREE(pr_list);
    }
  } else { /* ! if (pr_list) */
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
SLPError slp_discover_da(void)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPString   slp_string;
  SLPHeader   slp_header;

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_discover_da: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  if (!pkt) {
    return SLP_MEMORY_ALLOC_FAILED;
  }

  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;           /* version */
  slp_header.function_id = SLP_FUNCT_SRVRQST; /* request */
  slp_header.flags = SLP_FLAG_MCAST;          /* set multicast flag */
  slp_header.ext_offset = 0;                  /* Next Ext Offset */
  slp_header.xid = xid++;                     /* XID */
  slp_header.lang_tag.str = (char*)local_lang_str;
  slp_header.lang_tag.len = local_lang_len;
  offset = packSLP_HEADER(offset, &slp_header);

  /* PRList */
  slp_string.str = (char*)SLP_MALLOC(MAX_DA*17);      /* 16 bytes and a comma per item + null */
  if (slp_string.str) {
    da_ip_list(slp_string.str);
    slp_string.len = strlen(slp_string.str);
    offset = packSLP_STR(offset, &slp_string);
    acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: [%s] xid=%d",slp_string.str, slp_header.xid);
    SLP_FREE(slp_string.str);
  } else {
    acnlog(LOG_DEBUG | LOG_SLP, "slp_discover_da: Memory allocation error");
    return SLP_MEMORY_ALLOC_FAILED;
  }

  /* service-type */
  slp_string.len = da_req_len;
  slp_string.str = (char*)da_req_str;
  offset = packSLP_STR(offset, &slp_string);

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  /* predicate string(null) */
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  /* slp_spi (null) */
  offset = packSLP_STR(offset, &slp_string);

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* send it */
  /* create address */
  netx_INADDR(&dest_addr) = htonl(SLP_MCAST_ADDRESS);
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);
  return (SLP_OK);
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
/* FUNCTION NOT TESTED (for use without DA) */
SLPError slp_send_srvrply(ip4addr_t ip, uint16_t reply_xid, uint16_t error_code)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  void       *pkt;
  netx_addr_t dest_addr;

  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_srvrply: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* if we have and error code, we send it but no URL entry */

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVRPLY;       /* reply */
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = reply_xid;                       /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);

  /* Error code */
  offset = packUINT16(offset, error_code);

  /* pack URL unless there is an error */
  if (!error_code) {
    /* pack URL count (we only have one) */
    offset = packUINT16(offset,1);
    /* pack URL */
    slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
    /* TODO: this needs to get the URL from somewhere. Perhaps we can keep them in our attribute list */
    /*slp_urlentry.url.len = srv_url_len; */
    /*slp_urlentry.url.str = srv_url; */
    slp_urlentry.num_auth_blocks = 0;
    offset = packSLP_URL_ENTRY(offset, &slp_urlentry);
  } else {
    offset = packUINT16(offset,0);
  }

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif /* SLP_IS_SA */

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
SLPError slp_receive_srvrply(ip4addr_t ip, SLPHeader *header, char *data)
{
  /* Unpack error code, if OK, unpack URLs and call callback, for each URL?
   * In non-DA mode, these may arrive in multiple packets (multicast request).
   * Do we need to check to see if we already have this.
   * Only do this if a) we have a call back b) we have a matching XID
   * This is not thread aware so our sending routine sets the XID
   * and we clear it here.  Sending will fail if XID is already set.
   */

  uint16_t    error_code;
  uint16_t    url_count;
  SLPUrlEntry urlentry;
  SLPMsg     *Msg;
  char  *url;


  UNUSED_ARG(ip);

  LOG_FSTART();

  /* error, xid does not match, this may not be an error so return OK */
  Msg = msg_find_by_xid(header->xid);
  if (!Msg) {
    return SLP_OK;
  }

  data = unpackUINT16(data, &error_code);
  if (error_code) {
    /* callback with error code */
    if (Msg->callback) {
      Msg->callback(error_code, NULL, 0);
    }
  } else {
    /* get URL count */
    data = unpackUINT16(data, &url_count);
    /* TODO, should we have some sanity limit on the number of URLs to parse? */
    while (url_count) {
      /* callback with URL */
      data = unpackURL_ENTRY(data, &urlentry);
      /* rest of data void if we have authentication */
      if (urlentry.num_auth_blocks)
        break;
      /* urlentry contains a pointer to string data that is NOT terminated with a null */
      /* so we convert it. We MUST free this ourselves! */
      url = getSLP_STR(&urlentry.url);
      /* callback with URL here... */
      if (Msg->callback) {
        Msg->callback(SLP_OK, url, url_count);
      }
      SLP_FREE(url);
      url_count--;
    }
  }

  SLP_FREE(MSG_SRV_TYPE(Msg));
  SLP_FREE(MSG_PREDICATE(Msg));

  /* remove xid from our list */
  msg_free(Msg);

  return SLP_OK;
}
#endif /* SLP_IS_UA */

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
#if SLP_IS_SA
/*******************************************************************************/
/* Registers a service URL and service attributes with SLP.
 * TODO: current implementation only allows us to register one component
 *******************************************************************************/
SLPError slp_reg(char *reg_srv_url, char *reg_srv_type, char *reg_attr_list)
{
  acn_protect_t protect;
  SLPDa_list   *da;
  SLPRegProp   *reg_prop;

  LOG_FSTART();

  /* you must open first */
  if (!slp_socket)  {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_reg: socket not open");
    return SLP_NOT_OPEN;
  }

  /* make sure we have a real attribute list */
  if (!reg_attr_list) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_reg: !reg_prop");
    return SLP_PARAMETER_BAD;
  }

  /* make sure we have a real url */
  if (!reg_srv_url) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_reg: !reg_srv_url");
    return SLP_PARAMETER_BAD;
  }

  /* make sure we have a real type */
  if (!reg_srv_type) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_reg: !reg_srv_type");
    return SLP_PARAMETER_BAD;
  }

  protect = ACN_PORT_PROTECT();
  /* see if we already have this (could be a resend ) */
  reg_prop = reg_prop_find(reg_srv_url);
  if (!reg_prop) {
    reg_prop = reg_prop_add(reg_srv_url, reg_srv_type, reg_attr_list);
    if (!reg_prop) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_DEBUG | LOG_SLP , "slp_reg: out of registrion properties");
      return SLP_MEMORY_ALLOC_FAILED;
    }
  }

  /* send registration to all the DAs */
  da = da_list;
  while (da < da_list + MAX_DA) {
    /* if this DA has an IP */
    if(da->ip) {
      da_reg_prop(da,reg_prop,0);
    }
    da++;
  }
  ACN_PORT_UNPROTECT(protect);
  return SLP_OK;
}
#endif /* SLP_IS_SA */

/*******************************************************************************/
SLPError slp_send_reg(SLPMsg *Msg, bool fresh)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPString   slp_string;
  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_reg: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* create header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVREG;        /* register */
  if (fresh) {                                      /* flags (and reserved) */
    slp_header.flags = SLP_FLAG_FRESH;
  } else {
    slp_header.flags = 0;
  }
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = Msg->xid;                        /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);     /* copy data into the packet */

  /* url  */
  slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
  slp_urlentry.url.len = strlen(MSG_SRV_URL(Msg));
  slp_urlentry.url.str = MSG_SRV_URL(Msg);
  slp_urlentry.num_auth_blocks = 0;
  offset = packSLP_URL_ENTRY(offset, &slp_urlentry); /* copy data into the packet */

  /* service */
  slp_string.len = strlen(MSG_SRV_TYPE(Msg));
  slp_string.str = MSG_SRV_TYPE(Msg);
  offset = packSLP_STR(offset, &slp_string);         /* copy data into the packet */

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);         /* copy data into the packet */

  /* attr_list */
  slp_string.len = strlen(MSG_ATTR_LIST(Msg));
  slp_string.str = MSG_ATTR_LIST(Msg);
  offset = packSLP_STR(offset, &slp_string);         /* copy data into the packet */

  /* auth blocks    */
  offset = packUINT8(offset, 0); /* no auth blocks */

  /* adjust length data field and put in udp structure too */
  length = (unsigned long)offset - (unsigned long)data; /* find length of msg */
  offset = data + 2; /* point to where length will go, 2 bytes from start of msg */
  offset = packUINT24(offset, length);  /* put length in msg */


  /* create address */
  netx_INADDR(&dest_addr) = Msg->da->ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif /* SLP_IS_SA */

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
SLPError slp_dereg(char *reg_srv_url)
{
  acn_protect_t protect;
  SLPDa_list   *da;
  SLPRegProp   *reg_prop;

  LOG_FSTART();

  /* are we open? */
  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_dereg: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  protect = ACN_PORT_PROTECT();

  /* did we have a valid registration? */
  reg_prop = reg_prop_find(reg_srv_url);
  if (!reg_prop) {
    ACN_PORT_UNPROTECT(protect);
    acnlog(LOG_WARNING | LOG_SLP , "slp_dereg: matching registrion not found");
    return SLP_PARAMETER_BAD;
  }

  /* send registration to all the DAs */
  da = da_list;
  while (da < da_list + MAX_DA) {
    /* if this DA has an IP */
    if(da->ip) {
      da_dereg_prop(da, reg_prop);
      reg_prop_del(reg_prop);
    }
    da++;
  }
  ACN_PORT_UNPROTECT(protect);
  return SLP_OK;
}

/******************************************************************************/
SLPError slp_send_dereg(SLPMsg  *Msg)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  /* struct ip_addr ipaddr; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPString   slp_string;
  SLPHeader   slp_header;
  SLPUrlEntry slp_urlentry;

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_dereg: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVDEREG;      /* deregister */
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = Msg->xid;                        /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  /* URL Entry */
  slp_urlentry.lifetime = SLP_REGIGRATION_LIFETIME;
  slp_urlentry.url.len = strlen(MSG_SRV_URL(Msg));
  slp_urlentry.url.str = MSG_SRV_URL(Msg);
  slp_urlentry.num_auth_blocks = 0;
  offset = packSLP_URL_ENTRY(offset, &slp_urlentry);

  /* tag list (no tags) */
  offset = packUINT16(offset,0);

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = Msg->da->ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif /* SLP_IS_SA */

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
/* FUNCTION NOT TESTED - (for use as DA) */
SLPError slp_send_svrack(ip4addr_t ip, uint16_t reply_xid, uint16_t error_code)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  /* struct ip_addr ipaddr; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPHeader   slp_header;

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_svrack: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SRVACK;        /* register */
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = reply_xid;                       /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);

  /*  add error code */
  offset = packUINT16(offset, error_code);

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif /* SLP_IS_DA */


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
SLPError slp_attrrqst(ip4addr_t ip, char *req_url, char *req_tags, void (*callback) (int error, char *attributes, int count))
{
  SLPDa_list   *da;
  acn_protect_t protect;
  SLPMsg       *Msg;

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_attrrqst: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  protect = ACN_PORT_PROTECT();

  /* if ip = 0, then use first from list */
  if (!ip) {
    da = da_first();
    if (!da) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_DEBUG | LOG_SLP , "slp_attrrqst: No DA available");
      return(SLP_PARAMETER_BAD);
    }
  } else {
    da = da_find_by_ip(ip); 
    if (!da) {
      ACN_PORT_UNPROTECT(protect);
      acnlog(LOG_DEBUG | LOG_SLP , "slp_attrrqst: No DA available");
      return(SLP_PARAMETER_BAD);
    }
  }

  Msg = msg_new(xid++);
  if (Msg) {
    Msg->da = da;
    Msg->type = mtATTRRQST;
    Msg->counter = 0;
    Msg->retries = SLP_RETRIES;

    /* Yuck! copy strings */
    MSG_REQ_URL(Msg) = SLP_MALLOC(strlen(req_url) + 1);
    if (!MSG_REQ_URL(Msg)) {
      msg_free(Msg);
      ACN_PORT_UNPROTECT(protect);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_REQ_URL(Msg), req_url);

    MSG_TAGS(Msg) = SLP_MALLOC(strlen(req_tags) + 1);
    if (!MSG_TAGS(Msg)) {
      SLP_FREE(MSG_REQ_URL(Msg));
      msg_free(Msg);
      ACN_PORT_UNPROTECT(protect);
      return SLP_MEMORY_ALLOC_FAILED;
    }
    strcpy(MSG_TAGS(Msg), req_tags);

    Msg->callback = callback;
  } else {
    ACN_PORT_UNPROTECT(protect);
    return SLP_MEMORY_ALLOC_FAILED;
  }

  ACN_PORT_UNPROTECT(protect);
  return SLP_OK;
}

/******************************************************************************/
SLPError slp_send_attrrqst(SLPMsg *Msg)
{
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  /* struct ip_addr ipaddr; */
  void        *pkt;
  netx_addr_t  dest_addr;

  SLPString   slp_string;
  SLPHeader   slp_header;

  LOG_FSTART();

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_ATTRRQST;      /* register */
  slp_header.flags = 0;
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = Msg->xid;                           /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);

  /* predicate string(null) */
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  /* URL */
  if (MSG_REQ_URL(Msg)) {
    slp_string.len = strlen(MSG_REQ_URL(Msg));
  } else {
    slp_string.len = 0;
  }
  slp_string.str = MSG_REQ_URL(Msg);
  offset = packSLP_STR(offset, &slp_string);

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

  /* tag list */
  if (MSG_TAGS(Msg)) {
    slp_string.len = strlen(MSG_TAGS(Msg));
  } else {
    slp_string.len = 0;
  }
  slp_string.str = MSG_TAGS(Msg);
  offset = packSLP_STR(offset, &slp_string);

  /* slp_spi (null) */
  slp_string.len = 0;
  offset = packSLP_STR(offset, &slp_string);

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = Msg->da->ip;
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

  return (SLP_OK);
}
#endif /* SLP_IS_UA */

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
SLPError slp_receive_attrrply(ip4addr_t ip, SLPHeader *header, char *data)
{
  /* Unpack error code, if OK, unpack URLs and call callback, for each URL?
   * In non-DA mode, these may arrive in multiple packets (multicast request).
   * Do we need to check to see if we already have this.
   * Only do this if a) we have a call back b) we have a matching XID
   * This is not thread aware so our sending routine sets the XID
   * and we clear it here.  Sending will fail if XID is already set.
   */

  uint16_t error_code;
  /* uint16_t attr_length; */
  char       *attr_list;
  SLPString   slp_str;
  SLPMsg     *Msg;

  UNUSED_ARG(ip);

  LOG_FSTART();

  /* error, xid does not match, this may not be an error so return OK */
  Msg = msg_find_by_xid(header->xid);
  if (!Msg) {
    return SLP_OK;
  }

  data = unpackUINT16(data, &error_code);
  if (error_code) {
    /* callback with error code */
    if (Msg->callback) {
      Msg->callback(error_code, NULL, 0);
    }
  } else {
    /* get attributes count */
    data = unpackSLP_STR(data, &slp_str);
    /* TODO: should we get auth block and abort if it exist (we don't support) */

    /* get string */
    attr_list = getSLP_STR(&slp_str);
    /* callback */
    if (Msg->callback) {
      Msg->callback(error_code, attr_list, 0);
    }
    /* free memory */
    SLP_FREE(attr_list);
  }

  SLP_FREE(MSG_REQ_URL(Msg));
  SLP_FREE(MSG_TAGS(Msg));

  /* remove xid from our list */
  msg_free(Msg);

  return SLP_OK;
}
#endif /* SLP_IS_UA */

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
SLPError slp_receive_svrack(ip4addr_t ip, SLPHeader *header, char *data)
{
  uint16_t  error_code;
  SLPMsg    *Msg;

  UNUSED_ARG(header);
  UNUSED_ARG(ip);

  LOG_FSTART();

  /* error, xid does not match, this may not be an error so return OK */
  Msg = msg_find_by_xid(header->xid);
  if (!Msg) {
    return SLP_OK;
  }

  data = unpackUINT16(data, &error_code);
  if (error_code == SLP_OK) {
    /* process ack */
    da_service_ack(Msg->da);
  } else {
    /* DA reported an error, so we'll just nuke it */
    da_delete(Msg->da);
  }

  /* remove xid from our list */
  msg_free(Msg);

  return SLP_OK;
}
#endif /* SLP_IS_SA */


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
SLPError slp_receive_daadvert(ip4addr_t ip, SLPHeader *header, char *data)
{
SLPDAAdvert daadvert; 
char       *scope_list;

/*    3. UA uses multicast/convergence srvrqsts to discover DAs, then uses
 *       that set of DAs.  A UA that does not know of any DAs SHOULD retry
 *       DA discovery, increasing the waiting interval between subsequent
 *       attempts exponentially (doubling the wait interval each time.)
 *       The recommended minimum waiting interval is CONFIG_DA_FIND
 *       seconds.
 *
 *    4. A UA with no knowledge of DAs sends requests using multicast
 *       convergence to SAs.  SAs unicast replies to UAs according to the
 *       multicast convergence algorithm.

 * If message is multicast flag is set, the message is unsolicited (xid should be 0 too)
 * otherwise, these should be as a result of us sending a SereviceRequest and it
 * should have matching xid values

 * if error code is non-zero, rest of packet may not exist!

 * After discovering a new DA, a SA MUST wait a random time between 0
 * and CONFIG_REG_ACTIVE seconds before sending registration.

 * If boot time is 0, it is going down and remove from list.
 * Conversly only register to DA if bootime > 0

 * Only send reqistrations if the scope matches. If there is no scope, then
 * we should reply too

 * if we don't find any or we loose one, should we resend request for DA?

 *NOTE on OpenSLP library
 *  Unsolicited DAadverts have xid != 0;
 */

/*
   SAs MUST accept multicast service requests and unicast service
   requests.  SAs MAY accept other requests (Attribute and Service Type
   Requests).  SAs MUST listen for multicast DA Advertisements.

*/

  UNUSED_ARG(ip);

  LOG_FSTART();

  /* get the error code out of the message   */
  data = unpackUINT16(data, &daadvert.error_code);
  if (daadvert.error_code) {
    acnlog(LOG_ERR | LOG_SLP , "slp_receive_daadvert: error code %d",daadvert.error_code);
    return (SLPError)daadvert.error_code;
  }

  /* unpack the reset of the SLP advert message into the daadvert sructure */
  data = unpackUINT32(data, &daadvert.boot_time);
  data = unpackSLP_STR(data, &daadvert.url);
  data = unpackSLP_STR(data, &daadvert.scope_list);
  data = unpackSLP_STR(data, &daadvert.attr_list);

  /* we are going to ignore these fields for now... */
  #ifdef SLP_AUTH
  data = unpackSLP_STR(data, &daadvert.slp_sip);
  data = unpackUINT8(data, &x);
  daadvert.num_auth_blocks = x;
  while (x-- > 0) {
    data = unpackUINT16(data, &length);  /* descriptor */
    data = data + length;
  }
  #endif

  /* verify scope */
  scope_list = getSLP_STR(&daadvert.scope_list); /* move scope list to a real string */
  if (scope_list) {
    if (__strcasecmp(scope_list, acn_scope_str)) { /* if != "ACN-DEFAULT" */
      SLP_FREE(scope_list);
      acnlog(LOG_WARNING | LOG_SLP , "slp_receive_daadvert: exit, not scope != %s", acn_scope_str);
      return SLP_OK;
    }
    SLP_FREE(scope_list);
  } else {
    acnlog(LOG_WARNING | LOG_SLP , "slp_receive_daadvert: exit, no scope specified");
    return SLP_MEMORY_ALLOC_FAILED;
  }

  /* we have an "ACN-DEFAULT" DA advertisement */
  /* add the DA to our list */
  da_add(&daadvert);

  /* One should be able to tell a solicited advertisement from a
   * unsolicited one based on three flags. First, the xid for unsolicited
   * should be 0 and second, the unsolicited should have the multicast
   * flag set and three, the xid of the solicited one should match our
   * initial request. The OpenSLP library seem to have a bug where it does
   * do the first two. So, we will use the xid to match (this might be better
   * anyway. (one can also tell by a unicast destination address but stack
   * does not provide this value)
   */
  if (dda_timer.discover && dda_timer.xid && (header->xid == dda_timer.xid)) {
    /* Yep, so send a new one right away */
    acnlog(LOG_DEBUG | LOG_SLP , "slp_receive_daadvert: advertisement from our search: xid=%d", header->xid);
    dda_timer.counter = CONFIG_MC_MAX;
    dda_timer.xmits = 1;
    dda_timer.xid = xid;
    #if SLP_IS_UA || SLP_IS_SA
    acnlog(LOG_DEBUG | LOG_SLP , "slp_receive_daadvert: sending a new request: %d", dda_timer.xid);
    return slp_discover_da(); /* send a service request for da discovery */
    #endif
  }
  acnlog(LOG_DEBUG | LOG_SLP , "slp_receive_daadvert: unsolicited advertisement");

  return SLP_NOT_IMPLEMENTED;
}
#endif /* SLP_IS_UA || SLP_IS_SA */

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
/* FUNCTION NOT TESTED */
SLPError slp_send_saadvert(ip4addr_t ip, uint16_t reply_xid)
{
/*disabled for now only needed when running without DA available */
#if 0
  uint32_t     length;
  char        *data;
  char        *offset;
  /* SLPError     result; */
  /* struct ip_addr ipaddr; */
  void        *pkt;
  netx_addr_t  dest_addr;

  /* char        *url; */
  SLPString   slp_string;
  SLPHeader   slp_header;

  UNUSED_ARG(ip);

  LOG_FSTART();

  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_send_saadvert: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* we can't send advertizment if we don't have attribute list */
  if (!__attr_list_count()) {
    return SLP_INTERNAL_SYSTEM_ERROR;
  }

  /* SLPUrlEntry slp_urlentry; */

  /* create a new udp packet */
  pkt = netx_new_txbuf(0);
  /* get address where data will go */
  data = netx_txbuf_data(pkt);

  /* copy pointer */
  offset = data;

  /* add header */
  slp_header.version = SLP_VERSION;
  slp_header.function_id = SLP_FUNCT_SAADVERT;      /* SA Advertisement */
  slp_header.flags = SLP_FLAG_MCAST;                /* set multicast flag */
  slp_header.ext_offset = 0;                        /* Next Ext Offset */
  slp_header.xid = reply_xid;                       /* XID */
  slp_header.lang_tag.len = local_lang_len;         /* Language Tag Length */
  slp_header.lang_tag.str = (char*)local_lang_str;  /* Language Tag String */
  offset = packSLP_HEADER(offset, &slp_header);

   /* URL  */
   slp_string.len = sa_reg_len;
   slp_string.str = (char*)sa_reg_str;
  offset = packSLP_STR(offset, &slp_string);

  /* scope-list */
  slp_string.len = acn_scope_len;
  slp_string.str = (char*)acn_scope_str;
  offset = packSLP_STR(offset, &slp_string);

   /* attribute list  */
  slp_string.len = attr_list_len;
  slp_string.str = attr_list;
  offset = packSLP_STR(offset, &slp_string);

  /* auth blocks    */
  offset = packUINT8(offset, 0); /* no auth blocks */

  /* adjust length data field and put in udp structrure too */
  length = (unsigned long)offset - (unsigned long)data;
  offset = data + 2;
  offset = packUINT24(offset, length);  /* length */

  /* create address */
  netx_INADDR(&dest_addr) = htonl(SLP_MCAST_ADDRESS);
  netx_PORT(&dest_addr) = htons(SLP_RESERVED_PORT);

  /* send it */
  netx_send_to(slp_socket, &dest_addr, pkt, length);
  netx_release_txbuf(pkt);

#else
  UNUSED_ARG(ip);
  UNUSED_ARG(reply_xid);
#endif
  return (SLP_OK);

}
#endif /* SLP_IS_SA */

/*****************************************************************************/
/* desc   : callback for SLP
 * params : pkt, pointer to etherent packet packet
 *        : data, user defined data var from udp_open
 * returns: none
 */
/******************************************************************************/
void
slp_recv(netxsocket_t *socket, const uint8_t *data, int length, groupaddr_t group, netx_addr_t *source)
{
  /* slp_data* points to the data in the received UDP message */
  /* length is length of UDP data */
  /* ip_addr is that of the msg source */
  SLPHeader    slp_header;
  ip4addr_t    ip_addr;
  char         *slp_data;
  acn_protect_t protect;

  LOG_FSTART();

  UNUSED_ARG(socket);
  UNUSED_ARG(group);

  ip_addr = netx_INADDR(source);

  slp_data = (char*)data;

  /* make sure we have enough bytes.. */
  if (length > 4) {               /* need at least 4 bytes for SLP header */
    if (slp_data[0] == 2) {         /* verify version */
      /* acnlog(LOG_DEBUG | LOG_SLP , ("slp_recv: from %s: ", ntoa(ip_addr) ) );  */

      /* pass ptr to start of rx SLP msg and load 7 fields of header data into SLPHeader struct */
      slp_data = unpackSLP_HEADER(slp_data, &slp_header);

      protect = ACN_PORT_PROTECT();
      switch (slp_header.function_id) {
        case SLP_FUNCT_SRVRQST:           /* Service Request */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Request xid=%d", slp_header.xid);
          #if SLP_IS_SA
          /* commmented out because it does not work yet... */
          /* slp_receive_srvrqst(ip_addr, &slp_header, slp_data); */
          #endif
          break;
        case SLP_FUNCT_SRVRPLY:           /* Service Reply */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Reply");
          #if SLP_IS_UA
          slp_receive_srvrply(ip_addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_SRVREG:            /* Service Registration (DA only) */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Register");
          /* No Support Planned */
          break;
        case SLP_FUNCT_SRVDEREG:          /* Service Deregister   (DA only) */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Deregister");
          /* No Support Planned */
          break;
        case SLP_FUNCT_SRVACK:            /* Service Acknowledge */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Ack xid=%d",slp_header.xid);
          #if SLP_IS_SA
          slp_receive_svrack(ip_addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_ATTRRQST:          /* Attribute Request    */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Attr Request");
          /* TODO: Add support for this */
          break;
        case SLP_FUNCT_ATTRRPLY:          /* Attribute Reply */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Attr Reply");
          #if SLP_IS_UA
          slp_receive_attrrply(ip_addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_DAADVERT:          /* DA Advertisement */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: DA Advertisment xid=%d", slp_header.xid);
          #if SLP_IS_UA || SLP_IS_SA
          slp_receive_daadvert(ip_addr, &slp_header, slp_data);
          #endif
          break;
        case SLP_FUNCT_SRVTYPERQST:       /* Service Type Request */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Type Request");
          /* No Support Planned */
          break;
        case SLP_FUNCT_SRVTYPERPLY:       /* Service Type Reply */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Service Type Reply");
          /* No Support Planned */
          break;
        case SLP_FUNCT_SAADVERT:          /* SA Advertisement */
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: SA Advertisement");
          /* TODO: Support? */
          break;
        default:
          acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: Unknown function");
      } /* switch */
      ACN_PORT_UNPROTECT(protect);
    } else { /* version 2 only */
      acnlog(LOG_DEBUG | LOG_SLP , "slp_recv: unsupported version");
    }
  } /* enough data */
}


#if SLP_IS_UA || SLP_IS_SA
/*******************************************************************************

*******************************************************************************/
void  slp_active_discovery_start(void)
{
  uint16_t      tps_delay;
  acn_protect_t protect;


  /* "After a UA or SA restarts, its initial DA discovery request SHOULD be
   *  delayed for some random time uniformly distributed from 0 to
   *  CONFIG_START_WAIT seconds."

   * Get random number from 0 to CONFIG_START_WAIT.
   * Variable delay is a random number. We really don't care what bits, so
   * we will pick the top ones
   */
  LOG_FSTART();

  tps_delay = rand();
  while (tps_delay > CONFIG_START_WAIT) {
    tps_delay = tps_delay >> 1;
  }

  acnlog(LOG_DEBUG | LOG_SLP , "slp_active_discovery_start: delay %d",tps_delay);

  protect = ACN_PORT_PROTECT();
  dda_timer.counter = tps_delay;
  dda_timer.xmits = 0;
  dda_timer.xid = 0;
  dda_timer.discover = 1;
  ACN_PORT_UNPROTECT(protect);
}
#endif /* SLP_IS_UA || SLP_IS_SA */

/*******************************************************************************
srand() should be called before slp_init
*******************************************************************************/
void slp_init(void)
{
  static bool initialized_state = 0;

  if (initialized_state) {
    acnlog(LOG_INFO | LOG_SLP,"slp_init: already initialized");
    return;
  }
  initialized_state = 1;
  LOG_FSTART();

  /* initialize sub modules */
  nsk_netsocks_init();
  netx_init();

  /* sequential transaction id */
  xid = rand();

  /* clear da_list */
  memset(da_list, 0, sizeof(da_list));

  /* clear timer counts */
  memset(&dda_timer, 0, sizeof(SLPdda_timer));

  /* clear message buffers */
  PRINTF("size: %" PRIu32 ", %"PRIu32 "\n", (uint32_t)sizeof(SLPMsg), (uint32_t)sizeof(Msgs));
  memset(Msgs, 0, sizeof(Msgs));

  /* clear registration property buffers */
  memset(reg_props, 0, sizeof(reg_props));

  msticks = 0;
 
}

/*******************************************************************************
  start up SLP
*******************************************************************************/
SLPError slp_open(void)
{
  localaddr_t  localaddr;

  LOG_FSTART();

  /* sorry, we can only open once */
  if (slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_open: already open");
    return SLP_HANDLE_IN_USE;
  }

  /* wait for network to be running. DHCP will bring it up after we get a IP address */
  /* ... */

  /* set address */
  slpmcast = htonl(SLP_MCAST_ADDRESS);

  LCLAD_PORT(localaddr) = htons(SLP_RESERVED_PORT);

  /* get a netsock to track port usage*/
  slp_socket = nsk_new_netsock();
  if (slp_socket) {
    /* open port */
    if (netx_udp_open(slp_socket, LCLAD_ARG(localaddr)) != 0) {
      acnlog(LOG_DEBUG | LOG_SLP , "slp_open: could not open socket");
      nsk_free_netsock(slp_socket);
      slp_socket = NULL;
      return SLP_NETWORK_ERROR;
    }

    /* join multicast address */
    if (netx_change_group(slp_socket, slpmcast, netx_JOINGROUP) != 0) {
      netx_udp_close(slp_socket);
      nsk_free_netsock(slp_socket);
      slp_socket = NULL;
      acnlog(LOG_DEBUG | LOG_SLP , "slp_open: could not change group addres");
      return SLP_NETWORK_ERROR;
    }
    slp_socket->data_callback = slp_recv;
  } else {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_open: could not get netsock");
    return SLP_MEMORY_ALLOC_FAILED;
  }

  /* TODO: should we start thread to receive data here or do outside? */

  return SLP_OK;
}

/*******************************************************************************
  Shut down SLP
*******************************************************************************/
void slp_close(void)
{
  netxsocket_t *hold_socket;

  LOG_FSTART();

  /* can only open once */
  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_close: not open");
    return;
  }

  /* tell our DAs that we are going away (this is blocking) */
  #if SLP_IS_SA || SLP_IS_UA
  da_close();
  #endif

  /* remove our multicast address */
  netx_change_group(slp_socket, slpmcast, netx_LEAVEGROUP);

  /* TODO: we need to shut down timer here! */

  /* free our netsock */
  /* make sure no one is going to use it before we free it */
  hold_socket = slp_socket;
  /* mark use closed */
  slp_socket = NULL;
  /* close it */
  netx_udp_close(hold_socket);
  /* now get rid of it */
  nsk_free_netsock(hold_socket);

  return;
}


#if SLP_IS_SA
/*******************************************************************************/
/* TODO: Add support for more than one registration */
/*******************************************************************************/
SLPError slp_dereg_all(void)
{
  acn_protect_t protect;


  LOG_FSTART();

  /* are we open? */
  if (!slp_socket) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_dereg: SLP_NOT_OPEN:");
    return SLP_NOT_OPEN;
  }

  /* did we have a valid registration? */
  if (!reg_prop_count()) {
    acnlog(LOG_DEBUG | LOG_SLP , "slp_dereg: !nothing registered");
    return SLP_PARAMETER_BAD;
  }
  
  protect = ACN_PORT_PROTECT();
  /* say goodbye to all DA all */
  da_dereg_all();
  ACN_PORT_UNPROTECT(protect);
  return SLP_OK;
}
#endif /* SLP_IS_SA */


/* *****************************************************************************
 * debug routine to print DA list
 */
/*******************************************************************************/
void slp_stats(void)
{
  char *str;

  acnlog(LOG_INFO | LOG_STAT, "------------------------");
/* get some memeory */
  str = (char*)SLP_MALLOC(MAX_DA*17);      /* 16 bytes and a comma per item + null */
  /* get our list */
  da_ip_list(str);
  /* print it */
  acnlog(LOG_INFO | LOG_STAT , "DA: %s", str);
  /* release memory */
  SLP_FREE(str);
  acnlog(LOG_INFO | LOG_STAT , "REG_PROP: %d", reg_prop_count());
  acnlog(LOG_INFO | LOG_STAT , "MSG: %d", msg_count());
}

#endif /* CONFIG_SLP */
