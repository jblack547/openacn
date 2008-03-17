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

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* lwIP includes. */
#include "lwip/netifapi.h"

/* ACN includes */
#include "user_opt.h"
#include "types.h"
#include "uuid.h"
#include "acnlog.h"
#include "epi18.h"
#include "slp.h"
#include "ntoa.h"

/* DISCOVER include */
#include "discover.h"

// these (at least slp_cid) should be user configuratble...
static char my_cid[]  = "687742F1-C044-4406-8386-4FF432902FF6";
static char my_dcid[] = "ED3FB528-5D29-4C82-A371-D2561043BF17";
static char my_dmp[] = "c";  // c = ctl-access, d = dev-access, or cd = both

// examples of data...
//URL       : service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7634  
//Service   : service:acn.esta
//Scope     : ACN-DEFAULT
//Attr-list : (cid=7F508284-3FD2-4E5B-9AA4-CFE4777D7634),(acn-fctn=ETC AED),(acn-uacn=Unit 10),(acn-services=esta.dmp),(csl-esta.dmp=esta.sdt/10.8.41.241:4177;esta.dmp/d:77777777-3333-2222-3333-123456789090),(device-description=$:tftp://10.8.41.241/$.ddl)
//                 %s                                              %s                 %s                                                      %s          %d              %s                                                                 %s

static const char  acn_service_str[]  = "service:acn.esta";
static const char  acn_reg_str[]      = "service:acn.esta:///%s";
//
static const char  fctn_str[]         = "ETC AED";
static const char  uacn_str[]         = "Unit X"; // only part of string, we will append a unit number to it
static const char  attr_list_str[]    = 
"(cid=%s),\
(acn-fctn=%s),\
(acn-uacn=%s),\
(acn-services=esta.dmp),\
(csl-esta.dmp=esta.sdt/%s:%d;esta.dmp/%s:%s),\
(device-description=$:tftp://%s/$.ddl)";

//extern err_t ethernetif_init( struct netif *netif );

/* Local variables */
//static struct netif EMAC_if;


static void create_attr_list(char *new_attr_list) 
{
	char    *ip_str;

	if (new_attr_list) {
    ip_str = ntoa(netif_default->ip_addr.addr);
    sprintf(new_attr_list, attr_list_str, my_cid, fctn_str, uacn_str, ip_str, SDT_MULTICAST_PORT, my_dmp, my_dcid, ip_str);
    //                                    %s      %s        %s        %s        %d        %s     %s       %s
  }
}

static void create_url(char *new_url)
{
  sprintf(new_url, acn_reg_str, my_cid);
}

static void attrrqst_callback(int error, char *attr_list)
{
  if (!error) {
    printf("attrrqst callback: %s\n",attr_list);
    //slp_send_attrrqst(0, "service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7639","", s); 

  } else {
    printf("attrrqst callback: error %d\n",error);
  }
}

static void srvrqst_callback(int error, char *url)
{
  if (!error) {
    printf("srvrqs callback: %s\n",url);
    slp_send_attrrqst(0, "service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7639","csl-*,acn-fctn,acn-uacn", attrrqst_callback); 
//    slp_send_attrrqst(0, "service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7639","csl-esta.dmp,acn-fctn,acn-uacn", attrrqst_callback); 
    //slp_send_attrrqst(0, "service:acn.esta:///7F508284-3FD2-4E5B-9AA4-CFE4777D7639","*esta*", attrrqst_callback); 

  } else {
    printf("srvrqs callback: error %d\n",error);
  }
}

void discover_acn(char *dcid)
{
  slp_send_srvrqst(0, "service:acn.esta", "(csl-esta.dmp=*ED3FB528-5D29-4C82-A371-D2561043BF17)", srvrqst_callback);
}

