/*--------------------------------------------------------------------*/
/*

Copyright (c) 2008, Electronic Theatre Controsl, Inc.

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

  TODO: malloc() implementation
  TODO: Version that implements its own memory (hence it will copy) rather than use block from stack.

*/
/*--------------------------------------------------------------------*/

#include "opt.h"
#include "types.h"
#include "acn_port.h"
#include "acnlog.h"

#include "netxface.h"
#include "netsock.h"



/***********************************************************************************************/
netxsocket_t sockets_tbl[MAX_NSK_SOCKETS];


/***********************************************************************************************/
/*
  find the socket (if any) with matching local address
*/
netxsocket_t *
nsk_find_netsock(localaddr_t *localaddr)
{
  netxsocket_t *socket;

    /* get address to array of sockets */
  socket = sockets_tbl;
#if CONFIG_LOCALIP_ANY
  /* while the port num in this socket entry is not equal to the port num passed in */
  while (NSK_PORT(socket) != LCLAD_PORT(*localaddr)) {
#else
  while (NSK_PORT(socket) != LCLAD_PORT(*localaddr) || NSK_INADDR(socket) != LCLAD_INADDR(*localaddr)) {
#endif
    if (++socket >= sockets_tbl + MAX_NSK_SOCKETS) return NULL;
  }
  return socket;
}

/***********************************************************************************************/
/*
 * Create new netsock
 */
netxsocket_t *
nsk_new_netsock(void)
{
  netxsocket_t *socket;
  acn_protect_t protect;

    /* get address to array of sockets */
  socket = sockets_tbl;
  /* while this entry is in use (port number in it is non-zero) */
  protect = ACN_PORT_PROTECT();
  while (NSK_PORT(socket) != netx_PORT_NONE) {
      /* point to the next entry in the array */
    if (++socket >= sockets_tbl + MAX_NSK_SOCKETS) {
      ACN_PORT_UNPROTECT(protect);
      return NULL;
    }
  }
  NSK_PORT(socket) = netx_PORT_HOLD; /* give it a marker so we know it is used */
  ACN_PORT_UNPROTECT(protect);
  return socket;
}

/***********************************************************************************************/
/*
 * Free netsock
 */
void
nsk_free_netsock(netxsocket_t *socket)
{
  NSK_PORT(socket) = netx_PORT_NONE;
  socket->nativesock = 0;
  socket->data_callback = NULL;
}

/***********************************************************************************************/
/*
 * init module
 */
void
nsk_netsocks_init(void)
{
  static bool initialized_state = 0;
  netxsocket_t *socket;

  if (initialized_state) {
    acnlog(LOG_INFO | LOG_NSK,"nsk_netsocks_init: already initialized");
    return;
  }
  initialized_state = 1;
  acnlog(LOG_DEBUG|LOG_NSK,"nsk_netsocks_init");

  for (socket = sockets_tbl; socket < sockets_tbl + MAX_NSK_SOCKETS; ++socket) {
    NSK_PORT(socket) = netx_PORT_NONE;
    socket->nativesock = 0;
    socket->data_callback = NULL;
  }
}

/***********************************************************************************************/
/*
 * Next used socket
 */
netxsocket_t *
nsk_next_netsock(struct netxsocket_s *socket)
{
  while (++socket < sockets_tbl + MAX_NSK_SOCKETS) {
    if (NSK_PORT(socket) != netx_PORT_NONE) {
      return socket;
    }
  }
  return NULL;
}

/***********************************************************************************************/
/*
 * first used socket
 */
netxsocket_t *
nsk_first_netsock(void)
{
  return nsk_next_netsock(sockets_tbl - 1);
}



