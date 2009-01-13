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

/* Lib includes */
#include <stdlib.h>

/* General ACN includes */
#include "opt.h"
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"

#include "netxface.h"
#include "ntoa.h"
#include "aton.h"
#include "cid.h"
#include "epi18.h"
#include "slp.h"
#include "sdt.h"
#include "component.h"

/* this module */
#include "discover.h"


/* these (at least my_cid) should be user configurable... */
/* static char my_dmp[] = "c";  // c = ctl-access, d = dev-access, or cd = both */

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

/* must be at least one space */
static const char default_uacn[] = "uacn";
static const char default_fctn[] = "fctn";

/* TODO: NOTE! We only allow one active callback for this! */
static void (*discover_callback) (component_t *comp) = NULL;

/* Local variables */
/* Create an attribute list based on current parameters */
static void create_attr_list(char *new_attr_list, component_t *component) 
{
  char *ip_str;
  char cid_str[CID_STR_SIZE];
  char dcid_str[CID_STR_SIZE];
  char access_str[3] = {'\0'};
  const char *uacn;
  const char *fctn;

  /* if we received a non-zero pointer to the destination string */
  if (new_attr_list) {
    cidToText(component->cid, cid_str);
    cidToText(component->dcid, dcid_str);
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
		
    /* convert the IP string */
    ip_str = ntoa(netx_getmyip(NULL));

    /* don't let these be blank or slpd will croak */
    /* perhaps it would be better to just not include them if empty */
    if (component->fctn[0] != 0) {
      fctn = component->fctn;
    } else {
      fctn = default_fctn;
    }
    if (component->uacn[0] != 0) {
      uacn = component->uacn;
    } else {
      uacn = default_uacn;
    }
    
    /* create the attribute string for SLP discovery */
    sprintf(new_attr_list, attr_list_fmt, cid_str, fctn, uacn, ip_str, SDT_ADHOC_PORT, access_str, dcid_str, ip_str);
    /*                                    %s       %s    %s    %s      %d              %s          %s        %s */
  }
}

/* Create a ACN URL string from component cid */
static void create_url(char *new_url, component_t *component)
{
  char cid_str[CID_STR_SIZE];

  /* convert the CID to a string */
  cidToText(component->cid, cid_str);
  /* put the CID string into the URL string */
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
/* for DA builds */

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
      return FAIL;
    }
  }
  *attr_str = NULL;
  return OK;
}

static void attrrqst_callback(int error, char *attr_list, int count)
{
  cid_t cid = {0};
  cid_t dcid = {0};
  char cid_str[37]             = {'\0'};
  char fctn_str[ACN_FCTN_SIZE] = {'\0'};
  char uacn_str[ACN_FCTN_SIZE] = {'\0'};
  char ip_str[16]              = {'\0'};
  char port_str[6]             = {'\0'};
  char dd[64]                  = {'\0'};

#if CONFIG_SDT
  component_t *comp = NULL;
#endif

  ip4addr_t  ip = 0;
  ip4addr_t  myip = 0;
  ip4addr_t  mymask = 0;
  uint16_t  port = 0;

  char *next_attr;
  char *attr_str;
  char *p;
  char *s;
  char *e;

  if (!error) {
    acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: %s",attr_list);
    /* start from the first one */
    next_attr = attr_list;
    /* and rip thru each attribute block */
    while (get_attribute_str(&next_attr, &attr_str)) {
      /* 0 = match */
      /* CID ? */
      if (!strncmp(attr_str, "cid=", 4)) {
        strncpy(cid_str, attr_str+4, 36);
        textToCid(cid_str, cid);
        acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: cid=%s",cid_str);
        continue;
      }
      /* fctn ? */
      if (!strncmp(attr_str, "acn-fctn=", 9)) {
        strncpy(fctn_str, attr_str+9, 63);
        acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: fctn=%s",fctn_str);
        continue;
      }
      /* uacn ? */
      if (!strncmp(attr_str, "acn-uacn=", 9)) {
        strncpy(uacn_str, attr_str+9, 63);
        acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: uacn=%s",uacn_str);
        continue;
      }
      /* dmp ? if so, get dcid, ip and port */
      if (!strncmp(attr_str, "csl-esta.dmp=", 13)) {
        p = strstr(attr_str, "esta.dmp/");
        if (p) {
          p = strchr(p, ':');
          if (p) {
            strncpy(cid_str, p+1, 36);
            textToCid(cid_str, dcid);
            acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: dcid=%s",cid_str);
          }
        }
        /* these will look like this: esta.sdt/192.169.3.100:2487; */
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
              port = htons(atoi(port_str));
            }
            myip = netx_getmyip(0);
            mymask = netx_getmyipmask(0);
            if ((mymask & myip) == (mymask & ip)) {
              acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: reachable ip=%s, %s",ip_str, port_str);
              /* got what we need - move on */
              break;  /* from while(p) */
            } else {
              ip = 0;
              port = 0;
              break; /* from while(p) */
            }
          }
          /* see if there is another one in the same attribute (multiple nics)*/
          p = strstr(e, "esta.sdt/");
        }
        continue;
      }
      if (!strncmp(attr_str, "device-description=", 19)) {
        strncpy(dd, attr_str+19, 63);
        acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: dd=%s",dd);
        continue;
      }
    } /* of while */
  } else {
    acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: error %d",error);
    return;
  }

  if (!ip || !port) {
    acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: ip or port not correct");
    return;
  }

  /* create a component for this CID */
#if CONFIG_SDT
  if (!cidIsNull(cid)) {
    /* am I looking in the mirror ? */
    if (ip == myip) {
      acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: Look! I see can myself!");
      return;
    }
  	/* see if we already have this one */
    comp = sdt_find_component(cid);
    /* if it was found */
    if (comp) {
      acnlog(LOG_DEBUG | LOG_DISC , "attrrqst callback: component already exists");
      /*TODO: what should we do if we already have it? For now, just update ad-hoc*/
      netx_PORT(&comp->adhoc_addr) = port;
      netx_INADDR(&comp->adhoc_addr) = ip;
      comp->dirty = false;
    } else {
      /* create a new component struct at the end of the list */
      comp = sdt_add_component(cid, dcid, false, accUNKNOWN); 
      if (!comp) {
        return;
      }
      comp->created_by = cbDISC;
      netx_PORT(&comp->adhoc_addr) = (port);
      netx_INADDR(&comp->adhoc_addr) = (ip);
      strcpy(comp->fctn, fctn_str);
      strcpy(comp->uacn, uacn_str);
      /* let calling app know */
      if (discover_callback) {
        discover_callback(comp);
      }
    }
  }
#endif /* CONFIG_SDT */
}

#if SLP_IS_UA
/* for directory agent builds */
static void srvrqst_callback(int error, char *url, int count)
{
  if (!error) {
    acnlog(LOG_DEBUG | LOG_DISC , "srvrqst_callback: %s", url);

    /* TODO: do we want to get all attributes or just the ones we know about */
    /* slp_send_attrrqst(0, url,"cid,csl-esta.dmp,acn-fctn,acn-uacn", attrrqst_callback); */
    /* this will get all */
    slp_attrrqst(0, url, "", attrrqst_callback);
  } else {
    acnlog(LOG_DEBUG | LOG_DISC , "srvrqst_callback: error %d", error);
  }
}
#endif

#if SLP_IS_UA
/* starting point if we are a directory agent build */
void discover_acn(char *dcid_str, void (*callback) (component_t *component))
{
  char  predicate_str[100];
  cid_t dcid;
  component_t *comp;

  /* mark all our components as dirty so we know if we refound time */
  comp = sdt_first_component();
  while (comp) {
    if (cidIsEqual(comp->dcid, dcid)) {
      comp->dirty = true;
    }
    comp = comp->next;
  }


  /* TODO: we need an open() and close() to clear callback on shutdown? */
  /* perhaps we can auto-clear it with a timeout */
  discover_callback = callback;

  sprintf(predicate_str, predicate_fmt, dcid_str);
    
  slp_srvrqst(0, "service:acn.esta", predicate_str, srvrqst_callback);
}
#endif

/* (cid=01000000-0000-0000-0000-000000000001),(acn-fctn=),(acn-uacn=),(acn-services=esta.dmp),(csl-esta.dmp=esta.sdt/192.168.1.201:5568;esta.dmp/cd:02000000-0000-0000-0000-000000000002),(device-description=$:tftp://192.168.1.2) */
char  acn_attr_list[500] = {'\0'};
char  acn_srv_url[100] = {'\0'};
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

void discover_deregister(component_t *component)
{
  /* Create the URL string */
  create_url(acn_srv_url, component);
  slp_dereg(acn_srv_url);
}

