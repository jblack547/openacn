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
  Header file for slp.c
*/

#ifndef SLP_H
#define SLP_H

#include "opt.h"
#include "types.h"
#include "netxface.h"

/*=========================================================================*/
/* defines */
/*=========================================================================*/
#define SLP_UNUSED_ARG(x) (void)x

#define SLP_RESERVED_PORT       427
#define SLP_MCAST_ADDRESS       0xeffffffd  /* 239.255.255.253 */

#define SLP_VERSION             2
#define SLP_TTL					        16

/* #define SLP_THREAD_PRIO         3   */
/* #define SLP_THREAD_STACK        600 */ /* FIXME What should this be? */

#define SLP_IS_SA               1  /* service agent, provides services */
#define SLP_IS_UA               1  /* user agent, looks for services */

#define SLP_MTU                 600 /* Max SLP data packet (not including 44 bytes for header) */

#define MAX_RQST                50  /* Size of static list of simultaneous request that can be handled 
                                       for exmaple: if we are looking for a particular DCID and the DA
                                       tells use about 25 of them, then we will send off 25 attribute
                                       requests. So, this should be a few more than the number of devices
                                       expected to be found. 
                                    */

/*=========================================================================*/
/* SLP Constants                                                           */
/*=========================================================================*/
#define SLP_TMR_INTERVAL  100  /* ms */
#define SLP_TMR_TPS       (1000/SLP_TMR_INTERVAL) /* 10, inverse of SLP_TMR_INTERVAL */

/* TPS = number of times the timer is hit per second */
/* Max time to wait for a complete multicast query response (all values.) */
#define CONFIG_MC_MAX     ( 15 * SLP_TMR_TPS ) /* default: 15 seconds */
#define CONFIG_MC_RETRY_COUNT (3)              /* number of times we should retry a multicast message) */

/* Wait to perform DA discovery on reboot. */
#define CONFIG_START_WAIT	( 3 * SLP_TMR_TPS ) /* default: 3 seconds */

/* Wait interval before initial retransmission of multicast or unicast requests. */
#define CONFIG_RETRY      ( 2  * SLP_TMR_TPS ) /* default: 2 seconds */

/* Give up on unicast request retransmission. */
#define CONFIG_RETRY_MAX	( 15 * SLP_TMR_TPS ) /* default: 15 seconds */

/* DA Heartbeat, so that SAs passively detect new DAs. */
#define CONFIG_DA_BEAT    (  3*60*60 * SLP_TMR_TPS ) /* default: 3 hours */

/* Minimum interval to wait before repeating Active DA discovery. */
#define CONFIG_DA_FIND	( 15*60*60 * SLP_TMR_TPS ) /* default: 15 minutes */

/* Wait to register services on passive DA discovery. */
#define CONFIG_REG_PASSIVE	( 3 * SLP_TMR_TPS ) /* default: 1-3 seconds */

/* Wait to register services on active DA discovery. */
#define CONFIG_REG_ACTIVE	( 3 * SLP_TMR_TPS ) /* default: 1-3 seconds */

/* DAs and SAs close idle connections. */
#define CONFIG_CLOSE_CONN	( 5*60*60 * SLP_TMR_TPS ) /* default: 5 minutes */

#define SLP_DA_SERVICE_TYPE       "service:directory-agent"
#define SLP_SA_SERVICE_TYPE       "service:service-agent"

#define SLP_REGIGRATION_LIFETIME	(5*60 + 30)        /* valid for 5+ minutes, we will update 5 minutes (value is in seconds) */

#define SLP_REGIGISTION_REFRESH	(5*60 * SLP_TMR_TPS) /* must be less than SLP_REGIGRATION_LIFETIME (value is TPS ) */

#define SLP_RETRIES               2

/* SLP Function ID constants                                               */
#define SLP_FUNCT_SRVRQST         1  /* Service Request */
#define SLP_FUNCT_SRVRPLY         2  /* Service Reply */
#define SLP_FUNCT_SRVREG          3  /* Service Registration */
#define SLP_FUNCT_SRVDEREG        4  /* Service Deregister */
#define SLP_FUNCT_SRVACK          5  /* Service Acknowledge */
#define SLP_FUNCT_ATTRRQST        6  /* Attribute Request */
#define SLP_FUNCT_ATTRRPLY        7  /* Attribute Reply */
#define SLP_FUNCT_DAADVERT        8  /* DA Advertisement */
#define SLP_FUNCT_SRVTYPERQST     9  /* Service Type Request */
#define SLP_FUNCT_SRVTYPERPLY     10 /* Service Type Reply */
#define SLP_FUNCT_SAADVERT        11 /* SA Advertisement */

#define SLP_FLAG_OVERFLOW         0x8000
#define SLP_FLAG_FRESH            0x4000
#define SLP_FLAG_MCAST            0x2000

/*=========================================================================*/
/* Constants*/
/*=========================================================================*/

/*=========================================================================*/
/* Types and structs */
/*=========================================================================*/
typedef void* SLPHandle;

typedef enum {
  SLP_LIFETIME_DEFAULT = 10800,
  SLP_LIFETIME_MAXIMUM = 65535
} SLPURLLifetime;

typedef enum {
  /* internal errors */
  SLP_UNEXPECTED_MSG               = -14,
	SLP_NOT_OPEN                     = -13,
	SLP_DA_NOT_FOUND								 = -12,
	SLP_DA_LIST_FULL								 = -11,
  SLP_BUFFER_OVERFLOW              = -10,
  SLP_NETWORK_TIMED_OUT            = -9,
  SLP_NETWORK_INIT_FAILED          = -8,
  SLP_MEMORY_ALLOC_FAILED          = -7,
  SLP_PARAMETER_BAD                = -6,
  SLP_INTERNAL_SYSTEM_ERROR        = -5,
  SLP_NETWORK_ERROR                = -4,
  SLP_NOT_IMPLEMENTED              = -3,
  SLP_HANDLE_IN_USE                = -2,
  SLP_FAIL                         = -1,

  /* external errors */
  SLP_OK                           = 0,
  SLP_LANGUAGE_NOT_SUPPORTED       = 1,
  SLP_PARSE_ERROR                  = 2,
  SLP_INVALID_REGISTRATION         = 3,
  SLP_SCOPE_NOT_SUPPORTED          = 4,
  SLP_AUTHENTICATION_UNKNOWN       = 5,
  SLP_AUTHENTICATION_ABSENT        = 6,
  SLP_AUTHENTICATION_FAILED        = 7,
  SLP_VERSION_NOT_SUPPORTED		     = 9,
  SLP_INTERNAL_ERROR               = 10,
  SLP_DA_BUSY                      = 11,
  SLP_OPTION_NOT_UNDERSTOOD        = 12,
  SLP_INVALID_UPDATE               = 13,
  SLP_MSG_NOT_SUPPORTED            = 14,
  SLP_REFRESH_REJECTED             = 15

} SLPError ;


typedef enum {
   SLP_FALSE = 0,
   SLP_TRUE = 1
} SLPBoolean;


#define DA_CLOSED    		0  /* closed, no active registrions. */
#define DA_SEND_REG 		1  /* need to send registration */
#define DA_WAIT_REG 		2  /* waiting for ack from registration */
#define DA_EXPIRE   		3  /* timing out refresh registation */
#define DA_SEND_DEREG   4  /* waiting for ack from deregistration */
#define DA_WAIT_DEREG   5  /* send deregister message */

typedef struct _SLPDa_list
{
  ip4addr_t   ip;
  uint32_t    state;      /* for ack from da after registration */
  uint32_t    counter;    /* counter for message handling */
  uint32_t    retries;    /* number of times to retry to sending message; */
  uint32_t    boot_time;  /* boottime from discovery */
  uint32_t    timeout;    /* counter to discover missing gone bye bye! See CONFIG_DA_BEAT */
} SLPDa_list;

typedef struct _SLPdda_timer
{
  uint8_t     discover;			/* set to TRUE to start discovery */
  uint32_t    counter;  		/* counter */
  uint8_t	    xmits;        /* number of times we have sent message */
  uint16_t	  xid;          /* xid of the message */
} SLPdda_timer;

/*=========================================================================*/
/* NOTE, do NOT use string functions on SLPString structures, they do NOT contain a terminating NULL! */
typedef struct _SLPString
{
  uint16_t  len;
  char     *str;
}SLPString;

/*=========================================================================*/
typedef struct _SLPUrlEntry
{
  uint16_t    lifetime;
  SLPString   url;
  uint8_t     num_auth_blocks;
/*  SLPAuthBlk  auth_block; */
} SLPUrlEntry;

/*=========================================================================*/
/* SLPHeader structure and associated functions                            */
/* (not as in UDP packet)                                                  */
/*=========================================================================*/
typedef struct _SLPHeader
{
  uint8_t       version;
  uint8_t       function_id;
  uint32_t      length;
  uint16_t      flags;
  uint32_t      ext_offset;
  uint16_t      xid;
  SLPString     lang_tag;
}SLPHeader;

/*=========================================================================*/
typedef struct _SLPDAAdvert
{
  uint16_t    error_code;
  uint32_t    boot_time;
  SLPString   url;
  SLPString   scope_list;
  SLPString   attr_list;
  SLPString   slp_sip;
  uint8_t     num_auth_blocks;
/*  SLPAuthBlk  auth_block; */
}SLPDAAdvert;

/*=========================================================================*/
typedef struct _SLPSrvRegistation
{
  SLPUrlEntry url_entry;
  SLPString   service_type;
  SLPString   scope_list;
  SLPString   attr_list;
  uint8_t     num_auth_blocks;
/*  SLPAuthBlk  auth_block; */
}SLPSrvRegistration;

/*=========================================================================*/
typedef struct _SLPSrvRequest
{
  SLPString   pr_list;
  SLPString   service_type;
  SLPString   scope_list;
  SLPString   predicate;
  SLPString   slp_spi;
}SLPSrvRequest;

/*=========================================================================*/
typedef struct _SLPSrvAck
{
	uint16_t	error_code;
}SLPSrvAck;

typedef enum _SLPMsgType
{
  mtREG,
  mtDEREG,
  mtSRVRQST,
  mtATTRRQST
} SLPMsgType;

/*=========================================================================*/
typedef struct _SLPMsg
{
  SLPMsgType  type;       /* type of messages (registration, deregistration, attribute... */
  uint16_t    xid;        /* xid of message */
  int         retries;    /* numer of time the message can be sent before giving up. */
  int         counter;    /* counter to expire message */
  SLPDa_list *da;         /* DA it was sent to */
  char       *param1;     /* char* for message */
  char       *param2;     /* char* for message */
  char       *param3;     /* char* for message */
  void (*callback) (int error, char *url, int count);
  struct _SLPMsg  *next;
}SLPMsg;

/*=========================================================================*/
typedef struct _SLPRegProp
{
  char *reg_srv_url;
  char *reg_srv_type;
  char *reg_attr_list;
}SLPRegProp;


/*=========================================================================*/
/* GlobalDeclarations                                                            */
/*=========================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

/* slp thread */
void slp_thread( void *pvParameters );

/* slp init routine */
void slp_init(void);

/* close slp */
void slp_close(void);
void slp_close2(void);

/* open slp */
SLPError slp_open(void);

/* slp timer callback */
void slp_tick(void *arg);


#if SLP_IS_SA
/* slp registration */
SLPError slp_reg(char *reg_srv_url, char *reg_srv_type, char *reg_attr_list);
/* slp de-registration */
SLPError slp_dereg(char *reg_srv_url);
SLPError slp_dereg_all(void);
#endif  /* SLP_IS_SA */


#if SLP_IS_UA
SLPError slp_srvrqst(ip4addr_t ip, char *req_srv_type, char *req_predicate, void (*callback) (int error, char *url, int count));
SLPError slp_attrrqst(ip4addr_t ip, char *req_url, char *req_tags,      void (*callback) (int error, char *attributes, int count));
#endif


/* UA and SA functions */
#if SLP_IS_UA || SLP_IS_SA
void    slp_active_discovery_start(void);
void		slp_stats(void);

#endif

/* call back for UDP SLP receive */
/* void slp_recv(char *slp_data, int length, uint32_t ip_addr, int port); */
void slp_recv(netxsocket_t *socket, const uint8_t *data, int length, netx_addr_t *dest, netx_addr_t *source, void *ref);

#ifdef __cplusplus
}
#endif



#endif /* SLP_H */
