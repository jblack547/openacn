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
/*
This module is separated from the main RLP to allow customization of memory
handling. See rlp.c for description of 3-level structure.
*/
/* static const char *rcsid __attribute__ ((unused)) = */
/*   "$Id$"; */

#include <string.h>
#include "opt.h"
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include "netxface.h"
#include "netsock.h"
#include "marshal.h"
#include "acn_rlp.h"
#include "rlp.h"
#include "rlpmem.h"

#if CONFIG_RLPMEM_MALLOC
#include <stdlib.h>
#endif

/***********************************************************************************************/
/*
The socket/group/listener API is designed to allow dynamic allocation of
channels within groups and/or a linked list or other more sophisticated
implementation.

API
--
rlp_rxgroup_t *rlpm_find_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr)
void           rlpm_free_rxgroup(netxsocket_t *netsock, rlp_rxgroup_t *rxgroup)
rlp_rxgroup_t *rlpm_new_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr)

rlp_listener_t *rlpm_new_listener(rlp_rxgroup_t *rxgroup)
void            rlpm_free_listener(rlp_listener_t *listener)
rlp_listener_t *rlpm_next_listener(rlp_listener_t *listener, protocolID_t pdu_protocol)
rlp_listener_t *rlpm_first_listener(rlp_rxgroup_t *rxgroup, protocolID_t pdu_protocol)

int             rlpm_netsock_has_rxgroups(netxsocket_t *netsock)
int             rlpm_rxgroup_has_listeners(rlp_rxgroup_t *rxgroup)
rlp_rxgroup_t  *rlpm_get_rxgroup(rlp_listener_t *listener)
netxsocket_t   *rlpm_get_netsock(rlp_rxgroup_t *rxgroup)
void            rlpm_init(void)




*/
/***********************************************************************************************/
/* static rlp_listener_t *listeners = NULL; */ /* not used a this time*/
static rlp_rxgroup_t  *rxgroups  = NULL;

#if CONFIG_RLPMEM_STATIC
static rlp_txbuf_t     txbufs_m[MAX_TXBUFS];
static rlp_listener_t  listeners_m[MAX_LISTENERS];
static rlp_rxgroup_t   rxgroups_m[MAX_RXGROUPS];
#endif

/************************************************************************************************
 *
 * The following routines are alternate memory management routines for static or malloc desires!
 */
#if CONFIG_RLPMEM_STATIC
/***********************************************************************************************/
void __init_txbufs(void)
{
  rlp_txbuf_t  *txbuf;

  for (txbuf = txbufs_m; txbuf < txbufs_m + MAX_TXBUFS; ++txbuf) {
    txbuf->usage = 0;
  }
}

/***********************************************************************************************/
rlp_txbuf_t * __new_txbuf(void)
{
  static int bufnum = 0;
  int        i;

  /* start with last one */
  i = bufnum;
  while (txbufs_m[i].usage != 0) {
    /* try next */
    ++i;
    /* deal with wrapping */
    if (i >= MAX_TXBUFS) i = 0;
    /* have come back to the start */
    if (i == bufnum) return NULL;
  }
  /* track the last one we used */
  bufnum = i;

  return &txbufs_m[i];
}

/***********************************************************************************************/
void __free_txbuf(rlp_txbuf_t *txbuf)
{
  txbuf->usage = 0;
  txbuf->owner = NULL;
  txbuf->datap = NULL;
  txbuf->owner = NULL;
  txbuf->datasize = 0;
  txbuf->netbuf = NULL;
}

/***********************************************************************************************/
void __init_rxgroups(void)
{
  rlp_rxgroup_t *rxgroup;

  for (rxgroup = rxgroups_m; rxgroup < rxgroups_m + MAX_RXGROUPS; ++rxgroup) {
    rxgroup->socket = NULL;
  }
}

/***********************************************************************************************/
rlp_rxgroup_t *__new_rxgroup(void)
{
  rlp_rxgroup_t *rxgroup;

  for (rxgroup = rxgroups_m; rxgroup < rxgroups_m + MAX_RXGROUPS; ++rxgroup) {
    if (rxgroup->socket == NULL)  { /* NULL socket marks unused listener */
      return rxgroup;
    }
  }
  return NULL;
}

/***********************************************************************************************/
void __free_rxgroup(rlp_rxgroup_t *rxgroup)
{
  rxgroup->socket = NULL;
}


/***********************************************************************************************/
void __init_listeners(void)
{
  rlp_listener_t *listener;

  for (listener = listeners_m; listener < listeners_m + MAX_LISTENERS; ++listener) {
    listener->protocol = PROTO_NONE;
  }
}

/***********************************************************************************************/
rlp_listener_t *__new_listener(void)
{
  rlp_listener_t *listener;

  for (listener = listeners_m; listener < listeners_m + MAX_LISTENERS; ++listener) {
    if (listener->protocol == PROTO_NONE)  { /* PROTO_NONE marks unused listener */
      return listener;
    }
  }
  return NULL;
}

/***********************************************************************************************/
void __free_listener(rlp_listener_t *listener)
{
  listener->protocol = PROTO_NONE;
}

#elif CONFIG_RLPMEM_MALLOC

/***********************************************************************************************/
#define __init_txbufs()

/***********************************************************************************************/
rlp_txbuf_t * __new_txbuf(void)
{
  return malloc(sizeof (rlp_txbuf_t));
}

/***********************************************************************************************/
void __free_txbuf(rlp_txbuf_t *txbuf)
{
  free((void*)txbuf);
}

/***********************************************************************************************/
#define __init_rxgroups()

/***********************************************************************************************/
rlp_rxgroup_t *__new_rxgroup(void)
{
  return malloc(sizeof (rlp_rxgroup_t));
}

/***********************************************************************************************/
void __free_rxgroup(rlp_rxgroup_t *rxgroup)
{
  free((void*)rxgroup);
}

/***********************************************************************************************/
#define __init_listeners()

/***********************************************************************************************/
rlp_listener_t *__new_listener(void)
{
  return malloc(sizeof (rlp_listener_t));
}

/***********************************************************************************************/
void __free_listener(rlp_listener_t *listener)
{
  free((void*)listener);
}
#endif /* #elif CONFIG_RLPMEM_MALLOC */


/***********************************************************************************************/
rlp_rxgroup_t *
rlpm_first_rxgroup(void)
{
  return rxgroups;
}


/***********************************************************************************************/
/*
 * find a rxgroup associated with this socket which has the correct groupaddr
 */
rlp_rxgroup_t *
rlpm_find_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr)
/* groupaddr is IP address of the dest of the UDP msg we received */
{
  rlp_rxgroup_t *rxgroup;

  /* treat all non-multicast as the same */
  if (groupaddr != netx_GROUP_UNICAST && !is_multicast(groupaddr)) {
    groupaddr = netx_GROUP_UNICAST; /* set to zero */
  }

  rxgroup = rxgroups;
  while (rxgroup) {
    if (rxgroup->socket == netsock && rxgroup->groupaddr == groupaddr )  {
      return rxgroup; /* return first matching listener */
    }
    rxgroup = rxgroup->next;
  }

  return NULL;
}

/***********************************************************************************************/
/*
Free a listener group
Only call if group is empty (no listeners exist)
*/
void
rlpm_free_rxgroup(netxsocket_t *netsock, rlp_rxgroup_t *rxgroup)
{
  rlp_rxgroup_t  *this_rxgroup;
  rlp_listener_t *listener;
  rlp_listener_t *next_listener;
  UNUSED_ARG(netsock);

  /* if, for some reason, we have listeners, free them */
  /* since we are nuking all of them in the chain, this is faster than calling rlpm_free_listener() */
  listener = rxgroup->listeners;
  while(listener) {
    next_listener = listener;
    __free_listener(listener);
    listener = next_listener;
  }

  /* see if its the first one */
  if (rxgroup == rxgroups) {
    rxgroups = rxgroup->next;
    __free_rxgroup(rxgroup);
  } else {
    /* start from the top */
    this_rxgroup = rxgroups;
    while (this_rxgroup){
      if (this_rxgroup->next == rxgroup) {
        /* skip around the one we about to free */
        this_rxgroup->next = rxgroup->next;
        __free_rxgroup(rxgroup);
        return;
      }
      this_rxgroup = this_rxgroup->next;
    }
  }
}

/***********************************************************************************************/
/*
 * "Create" a new empty listener group and associate it with a socket and groupaddr
 */
rlp_rxgroup_t *
rlpm_new_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr)
{
  rlp_rxgroup_t *rxgroup;

  /* see if we already have it */
  if ((rxgroup = rlpm_find_rxgroup(netsock, groupaddr))) {
    return rxgroup;  /* found existing matching group */
  }

  /* allocate memory */
  rxgroup = __new_rxgroup();

  /* did get memory fail */
  if (!rxgroup) {
    return NULL;
  }

  /* set our values */
  rxgroup->socket = netsock;
  rxgroup->groupaddr = groupaddr; /* IP address, zero for unicast */

  /* put it at the head or our list */
  rxgroup->next = rxgroups;
  rxgroups = rxgroup;

  /* return pointer to new listeners[] entry */
  return rxgroup;
}

/***********************************************************************************************/
/*
 * "Create" a new empty (no associated protocol) listener within a group
 */
rlp_listener_t *
rlpm_new_listener(rlp_rxgroup_t *rxgroup)
{
  rlp_listener_t *listener;

  listener = __new_listener();

  /* did get memory fail fail */
  if (!listener) {
    return NULL;
  }

  /* set our values */
  listener->rxgroup = rxgroup;
  listener->protocol = PROTO_NONE;

  /* put it at the head of the list */
  listener->next = rxgroup->listeners;
  rxgroup->listeners = listener;

  return listener;
}

/***********************************************************************************************/
/*
 * Free an unused listener
 */
void
rlpm_free_listener(rlp_listener_t *listener)
{
  rlp_listener_t  *this_listener;
  rlp_rxgroup_t   *rxgroup;

  rxgroup = listener->rxgroup;

  if (rxgroup) {
    /* take the listener out of the list */
    if (listener == rxgroup->listeners) {
      rxgroup->listeners = listener->next;
      __free_listener(listener);
    } else {
      this_listener = rxgroup->listeners;
      /* find the one in front of this one and skip around it */
      while (this_listener) {
        if (this_listener->next == listener) {
          this_listener->next = listener->next;
          /* now free ours */
          __free_listener(listener);
          return;
        }
        this_listener = this_listener->next;
      }
    }
  } else {
    /* it is possible that someone freed the group first, in that case, we can just nuke the listener */
    __free_listener(listener);
  }
}

/***********************************************************************************************/
/*
 * Find the next listener in a group with a given protocol
 */
rlp_listener_t *
rlpm_next_listener(rlp_listener_t *listener, protocolID_t pdu_protocol)
{
  rlp_listener_t *alistener;

  alistener = listener->next;
  while (alistener) {
    if (alistener->protocol == pdu_protocol) {
      return alistener;
    }
    alistener = alistener->next;
  }
  return NULL;
}

/***********************************************************************************************/
/*
 * Find the first listener in a group with a given protocol
 */
rlp_listener_t *
rlpm_first_listener(rlp_rxgroup_t *rxgroup, protocolID_t pdu_protocol)
{
  rlp_listener_t *listener;

  listener = rxgroup->listeners;
  while (listener) {
    if (listener->protocol == pdu_protocol) {
      return listener;
    }
    listener = listener->next;
  }
  return NULL;
}

/***********************************************************************************************/
/*
 * true if a socket has channelgroups
 */
int
rlpm_netsock_has_rxgroups(netxsocket_t *netsock)
{
  rlp_rxgroup_t *rxgroup;

  rxgroup = rxgroups;
  while (rxgroup) {
    if (rxgroup->socket == netsock) {
      return RLP_OK;
    }
    rxgroup = rxgroup->next;
  }
  return RLP_FAIL;
}

/***********************************************************************************************/
/*
 * true if a rxgroup is not empty
 */
int
rlpm_rxgroup_has_listeners(rlp_rxgroup_t *rxgroup)
{
  if (rxgroup->listeners) {
    return RLP_OK;
  }
  return RLP_FAIL;
}

/***********************************************************************************************/
/*
 * Get the group containing a given listener
 */
#define rlpm_get_rxgroup(listener) (listener->rx_group)

/***********************************************************************************************/
/*
 * Get the netSocket containing a given group
 */
#define rlpm_get_netsock(rxgroup) (rxgroup->socket)


/***********************************************************************************************/
void
rlpm_init(void)
{
  static bool initialized_state = 0;

  if (initialized_state) {
    acnlog(LOG_INFO | LOG_SDT,"rlpm_init: already initialized");
    return;
  }
  initialized_state = 1;
  acnlog(LOG_DEBUG|LOG_RLPM, "rlpm_init...");

  /* initialize sub modules */
  nsk_netsocks_init();
  netx_init();

  /* and our memory */
  __init_txbufs();
  __init_rxgroups();
  __init_listeners();

}

/***********************************************************************************************/
/*
Transmit network buffer API
(note receive buffers may be the same thing internally but are not externally treated the same)
Actual Network buffers are platform dependent are referenced as void pointers here
Client protocols obtain network buffers using rlpm_newtxbuf(), rlpm_freetxbuf() and rlpm_releasetxbuf()
A pointer to the data inside the network buffer in maintained in the "datap" member.

When calling RLP to transmit data, they pass both the pointer to the buffer and a pointer to the
data to send which need not start at the beginning of the buffers data area.
This allows for example, a higher protocol to re-transmit only a part of a former packet.
However, in this case, the content of the data area before that passed may be overwritten by rlp.

To track usage the protocol can use rlp_incuse(buf) and rlp_decuse(buf). Also rlp_getuse(buf)
which will be non zero if the buffer is used. These can generally be implemented as macros.

rlpm_freetxbuf will only actually free the buffer if usage is zero.

*/
/***********************************************************************************************/
rlp_txbuf_t *
rlpm_new_txbuf(int size, component_t *owner)
{
  uint8_t     *datap;
  void        *netbuf; /*void to keep it platform dependent */
  rlp_txbuf_t *txbuf;

  /* get one of our managment structures */
  txbuf = __new_txbuf();
  if (!txbuf) {
    return NULL;
  }
  /* Get network buffer */
  netbuf = netx_new_txbuf(size);
  if (!netbuf) {
    /* free buffer */
    __free_txbuf(txbuf);
    return NULL;
  }

  /* Get pointer to data in network buffe*/
  datap = (uint8_t*)netx_txbuf_data(netbuf);
  if (!datap) {
    /* didn't use it, free it what we created*/
    netx_free_txbuf(netbuf);
    __free_txbuf(txbuf);
    return NULL;
  }

  /* all set, now set the data values */
  txbuf->netbuf = netbuf;
  txbuf->datap = datap;
  txbuf->owner = owner;
  txbuf->datasize = MAX_MTU;
  txbuf->usage = 1;

  return txbuf;
}

/***********************************************************************************************/
/* called if data is not sent and we need to free the network memory */
void
rlpm_free_txbuf(rlp_txbuf_t *txbuf)
{
  /* release memory back to stack */
  netx_free_txbuf(txbuf->netbuf);
  /*free our memory */
  __free_txbuf(txbuf);
}

/***********************************************************************************************/
/* called after data is sent. network memory release is dealt with at the platform level*/
void
rlpm_release_txbuf(rlp_txbuf_t *txbuf)
{
  rlp_decuse(txbuf);
  /*free network buffer if no longer used */
  if (txbuf->usage == 0) {
    __free_txbuf(txbuf);
  }
}


#ifdef NEVER /* (save, may need for static implemention of netx) */
/***********************************************************************************************/
struct
rlp_txbuf_s *rlpm_new_txbuf(int size, component_t *owner)
{
  uint8_t *buf;

  buf = malloc(
      sizeof(struct rlp_txbuf_s)
      + RLP_PREAMBLE_LENGTH
      + sizeof(protocolID_t)
      + sizeof(cid_t)
      + size
      + RLP_POSTAMBLE_LENGTH
    );
  if (buf != NULL)
  {
    rlp_getuse(buf) = 0;
    ((struct rlp_txbuf_s *)buf)->datasize = RLP_PREAMBLE_LENGTH
        + sizeof(protocolID_t)
        + sizeof(cid_t)
        + size
        + RLP_POSTAMBLE_LENGTH;
    ((struct rlp_txbuf_s *)buf)->owner = owner;
  }

  return buf;
}

/***********************************************************************************************/
void rlpm_free_txbuf(struct rlp_txbuf_s *buf)
{
  rlp_decuse(buf);
  if (!rlp_getuse(buf)) free(buf);
}

#define rlpItsData(buf) ((uint8_t *)(buf) \
      + sizeof(struct rlpTxbufhdr_s) \
      + RLP_PREAMBLE_LENGTH \
      + sizeof(protocolID_t) \
      + sizeof(cid_t))

#endif


