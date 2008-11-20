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


//5.1.1.	Consoles
//Under the well known DCID (D1F6F109-8A48-4435-8157-A226604DEA89):

//5.1.2.	RFRs
//Under the well known DCID (CAB91A8C-CC44-49b1-9398-1EF5C07C31C9):

//TODO: change printf to acnlog...

/* Lib includes */
#include <stdlib.h>

/* General ACN includes */
#include "opt.h"
#include "types.h"
#include "acn_arch.h"
#include "acnlog.h"

#include "netxface.h"
#include "inet.h"
#include "uuid.h"
#include "epi18.h"
#include "slp.h"
#include "sdt.h"
#include "component.h"

/* this module */
#include "discover.h"



// these (at least my_cid) should be user configurable...
//static char my_dmp[] = "c";  // c = ctl-access, d = dev-access, or cd = both

/* examples of data... */
/*
  URL       : service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7634  
  Service   : service:acn.esta
  Scope     : ACN-DEFAULT
  Attr-list : (cid=7F508284-3FD2-4E5B-9AA4-CFE4777D7634),(acn-fctn=ETC AED),(acn-uacn=Unit 10),(acn-services=esta.dmp),(csl-esta.dmp=esta.sdt/10.8.41.241:4177;esta.dmp/d:77777777-3333-2222-3333-123456789090),(device-description=$:tftp://10.8.41.241/$.ddl)
                   %s                                              %s                 %s                                                      %s          %d              %s                                                                 %s
*/

/* helper strings */
static const char  acn_service_str[]  = "service:acn.esta";
static const char  acn_reg_fmt[]      = "service:acn.esta:///%s";
static const char  attr_list_fmt[]    = 
"(cid=%s),\
(acn-fctn=%s),\
(acn-uacn=%s),\
(acn-services=esta.dmp),\
(csl-esta.dmp=esta.sdt/%s:%d;esta.dmp/%s:%s),\
(device-description=$:tftp://%s/$.ddl)";
static const char predicate_fmt[]  = "(csl-esta.dmp=*%s)";


/* Local variables */

/* Create an attribute list based on current parameters */
static void create_attr_list(char *new_attr_list, component_t *component) 
{
  char *ip_str;
  char cid_str[UUID_STR_SIZE];
  char dcid_str[UUID_STR_SIZE];
  char access_str[3] = {'\0'};

  // if we received a non-zero pointer to the destination string
  if (new_attr_list) {
    uuidToText(component->cid, cid_str);
    uuidToText(component->dcid, dcid_str);
    switch (component->access) {
      case accDEVICE:
        access_str[0] = 'd';
        break;
      case accCONTROL:
        access_str[0] = 'c';
        break;
      case accBOTH:
        access_str[0] = 'c';
        access_str[1] = 'd';
        break;
      case accUNKNOWN:
        break;
    }
		
    // convert the IP string
    ip_str = ntoa(netx_getmyip(0));
    
    // create the attribute string for SLP discovery
    sprintf(new_attr_list, attr_list_fmt, cid_str, component->fctn, component->uacn, ip_str, SDT_ADHOC_PORT, access_str, dcid_str, ip_str);
    //                                    %s       %s               %s               %s      %d                 %s           %s        %s
  }
}

/* Create a ACN URL string from component cid */
static void create_url(char *new_url, component_t *component)
{
  char cid_str[UUID_STR_SIZE];

  // convert the CID to a string
  uuidToText(component->cid, cid_str);
  // put the CID string into the URL string
  sprintf(new_url, acn_reg_fmt, cid_str);
}

/* examples of attribute lists */
/* 
(cid=88D022B4-60E3-4CF8-9C14-44C64947E32F),
(csl-esta.dmp=esta.sdt/192.168.1.103:2732;esta.dmp/cd:D1F6F109-8A48-4435-8157-A226604DEA89),
(acn-fctn=ETC RFR Sink),
(acn-uacn=RFRSink)

(cid=7F508284-3FD2-4E5B-9AA4-CFE4777D7639),
(acn-fctn=ETC test device),(acn-uacn=Pokey),
(acn-services=esta.dmp),
(csl-esta.dmp=esta.sdt/192.169.3.100:2487;esta.dmp/d:77777777-3333-2222-3333-123456789090,
              esta.sdt/192.168.15.13:2486;esta.dmp/d:77777777-3333-2222-3333-123456789090,
              esta.sdt/10.8.41.229:2485;esta.dmp/d:77777777-3333-2222-3333-123456789090),
(device-escription=$:tftp://10.8.41.229/$.ddl,
                   $:tftp://192.168.15.13/$.ddl,
                   $:tftp://192.169.3.100/$.ddl)
*/
/* Callback when we receive a reply from our attribute request */
// for DA builds

bool get_attribute_str(char** next_attr, char**attr_str)
{
  char *s;
  char *e;
  /* find '(' */
  s = strchr(*next_attr, '(');
  if (s) {
    s = s + 1;
    /* now find mataching */
    e = strchr(s, ')');
    if (e) {
      *next_attr = e + 1;
      *e = 0;
      *attr_str = s;
      return 1;
    }
  }
  *attr_str = NULL;
  return 0;
}

static void attrrqst_callback(int error, char *attr_list)
{
  uuid_t  cid;
  uuid_t  dcid;
  char cid_str[37]             = {'\0'};
  char fctn_str[ACN_FCTN_SIZE] = {'\0'};
  char uacn_str[ACN_FCTN_SIZE] = {'\0'};
  char ip_str[16]              = {'\0'};
  char port_str[6]             = {'\0'};
  char dd[64]                  = {'\0'};

  component_t *comp = NULL;

  uint32_t  ip = 0;
  uint16_t  port = 0;

  char *next_attr;
  char *attr_str;
  char *p;
  char *s;
  char *e;

  /* local function to get attribute block */
  //TODO: do we need auto below
#ifdef NEVER
  auto bool get_attribute_str(void);
  bool get_attribute_str(void)
  {
    char *s;
    char *e;
    /* find '(' */
    s = strchr(next_attr, '(');
    if (s) {
      s = s + 1;
      /* now find mataching */
      e = strchr(s, ')');
      if (e) {
        next_attr = e + 1;
        *e = 0;
        attr_str = s;
        return 1;
      }
    }
    attr_str = NULL;
    return 0;
  }
#endif

  if (!error) {
    printf("attrrqst callback: %s\n",attr_list);
    /* start from the first one */
    next_attr = attr_list;
    /* and rip thru each attribute block */
    while (get_attribute_str(&next_attr, &attr_str)) {
      /* 0 = match */
      /* CID ? */
      if (!strncmp(attr_str, "cid=", 4)) {
        strncpy(cid_str, attr_str+4, 36);
        textToUuid(cid_str, cid);
        printf("cid: %s\n",cid_str);
        continue;
      }
      /* fctn ? */
      if (!strncmp(attr_str, "acn-fctn=", 9)) {
        strncpy(fctn_str, attr_str+9, 63);
        printf("fctn: %s\n",fctn_str);
        continue;
      }
      /* uacn ? */
      if (!strncmp(attr_str, "acn-uacn=", 9)) {
        strncpy(uacn_str, attr_str+9, 63);
        printf("fctn: %s\n",uacn_str);
        continue;
      }
      /* dmp ? if so, get dcid, ip and port */
      if (!strncmp(attr_str, "csl-esta.dmp=", 13)) {
        p = strstr(attr_str, "esta.dmp/");
        if (p) {
          p = strchr(p, ':');
          if (p) {
            strncpy(cid_str, p+1, 36);
            textToUuid(cid_str, dcid);
            printf("dcid: %s\n",cid_str);
          }
        }
        //these will look like this: esta.sdt/192.169.3.100:2487;
        p = strstr(attr_str, "esta.sdt/");
        while (p) {
          s = p + 9;
          e = strchr(s, ':');
          if (!e) {
            /* somethings not right - abort loop*/
            break; /* from while(p) */
          } else {
            /* get ip */
            strncpy(ip_str, s, e-s);
            ip = aton(ip_str);
            /* got ip, now get port */
            s = e+1;
            e = strchr(s, ';');
            if (!e) {
              /* somethings not right - abort loop*/
              ip = 0;
              break; /* from while(p) */
            } else {
              strncpy(port_str, s, e-s);
              port = atoi(port_str);
            }
            if ((netx_getmyipmask(0) & netx_getmyip(0)) == (netx_getmyipmask(0) & ip)) {
              printf("reachable ip: %s, %s\n",ip_str, port_str);
              /* got what we need - move on */
              break;  /* from while(p) */
            } else {
              ip = 0;
              port = 0;
            }
          }
          /* see if there is another one in the same attribute (multiple nics)*/
          p = strstr(e, "esta.sdt/");
        }
        continue;
      }
      if (!strncmp(attr_str, "device-description=", 19)) {
        strncpy(dd, attr_str+19, 63);
        printf("dd: %s\n",dd);
        continue;
      }
    }
  } else {
    printf("attrrqst callback: error %d\n",error);
  }

  /* create a component for this CID */
#if CONFIG_SDT
  if (!uuidIsNull(cid)) {
  	// use the CID to get the address of the component structure 
    //why is this commeted out?
    //comp = sdt_find_component(cid);  // COMMENTED OUT
    // if it was found
    if (comp) {
      uuidCopy(comp->dcid, dcid);
      netx_PORT(&comp->adhoc_addr) = htons(port);
      netx_INADDR(&comp->adhoc_addr) = htonl(ip);
    } else {
      /* create a new component struct at the end of the list */
      comp = sdt_add_component(cid, dcid, false, accUNKNOWN); 
      if (!comp) {
        return;
      }
    }
    netx_PORT(&comp->adhoc_addr) = htons(port);
    netx_INADDR(&comp->adhoc_addr) = htonl(ip);
    strcpy(comp->fctn, fctn_str);
    strcpy(comp->uacn, uacn_str);
  }
#endif /* CONFIG_SDT */
}

#if SLP_IS_UA
// for directory agent builds
static void srvrqst_callback(int error, char *url)
{
  if (!error) {
    printf("srvrqs callback: %s\n",url);
    //TODO: do we want to get all attributes or just the ones we know about
//    slp_send_attrrqst(0, url,"cid,csl-esta.dmp,acn-fctn,acn-uacn", attrrqst_callback); 
    // this will get all
    slp_send_attrrqst(0, url, NULL, attrrqst_callback);
  } else {
    printf("srvrqs callback: error %d\n",error);
  }
}
#endif

#if SLP_IS_UA
// starting point if we are a directory agent build
void discover_acn(char *dcid_str)
{
  char predicate_str[100];

  sprintf(predicate_str, predicate_fmt, dcid_str);
    
    // TODO: used dcid, not hard coded...
  slp_send_srvrqst(0, "service:acn.esta", predicate_str, srvrqst_callback);
}
#endif

//(cid=01000000-0000-0000-0000-000000000001),(acn-fctn=),(acn-uacn=),(acn-services=esta.dmp),(csl-esta.dmp=esta.sdt/192.168.1.201:5568;esta.dmp/cd:02000000-0000-0000-0000-000000000002),(device-description=$:tftp://192.168.1.2)
char  acn_attr_list[500] = {'\0'};
char  acn_srv_url[100] = {'\0'};
//TODO: support for multiple componets
//      prevent duplicate calls
// starting point if we are a device build (not a directory agent)
void discover_register(component_t *component)
{
  /* passed in component must have defined: 
     .cid
     .dcid
     .acccess = accDEVICE
     .uacn
     .fctn
  */

  /* Create the URL string */
  create_url(acn_srv_url, component);
  
  /* Create our attribute list */
  create_attr_list(acn_attr_list, component);
  
  /* register our list with a directory agent */
  slp_reg(acn_srv_url, (char*)acn_service_str, acn_attr_list);

}

/*
void discover_deregister(void)
{
  slp_dereg();
}
*/

