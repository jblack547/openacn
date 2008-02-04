/*--------------------------------------------------------------------*/
/* 

Copyright (c) 2007, Stuart Coyle.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of Stuart Coyle. nor the names of other
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

*/

#ifndef __sdt_handler_h__
#define __sdt_handler_h__ 1
/* 
   sdt_handler.h 
   Client protocol handler management for sdt
   In all functions "handlers" refers to the handler list. 
   If this is shared between threads we will need a locking mechanism.

   Client protocols can register handlers with:
   register_handlers(protocol,rx_handler,msg_handler, handlers) - 
       This registers callbacks for recieved packets and for sdt messages to the client protocol.
       Returns 0 if all is ok, returns -1 if handlers could not be allocated.
       TODO: more errors such as already registered protocol.

   deregister_handlers(protocol, handlers);
       removes the protocol from the handlers list. 

   get_rx_handler(protocol) - returns a pointer to the appropriate callback for protocol.
                              returns NULL if protocol is not registered.

   get_msg_handler(protocol) - returns a pointer to the message handler callback. 
                               returns NULL if not registered.

   init_handlers() - Creates and returns a new handler list.

   free_handlers(handlers) - Call on cleanup to free handlers memory.
*/


/* RX callback 
   arguments: local component, foreign component, data, data length
*/

#include "pp_acn_config.h"

typedef uint32_t (*rx_handler_t) (foreign_component_t *srcComp, local_component_t *dstComp, uint8_t *data, uint32_t datalength);

/* Message callback 
   TODO decide on a format for messaging to client protocols 
*/
typedef uint32_t (*msg_handler_t)(foreign_component_t *srcComp, local_component_t *dstComp, uint8_t *message);

typedef struct handler_t
{
  struct handler_t *next;
  struct handler_t *prev;
  protocolID_t protocol;
  rx_handler_t *rx_handler;
  msg_handler_t *msg_handler;
} handler_t;

rx_handler_t getRXHhandler(protocolID_t protocol, handler_t *handlers);
rx_handler_t getMsgHhandler(protocolID_t protocol, handler_t *handlers);

int registerHandlers(protocolID_t protocol, rx_handler_t *rx_handler, msg_handler_t *msg_handler, handler_t *handlers);
int deregisterHandlers(protocolID_t protocol, handler_t *handlers);
int initHandlers(handler_t *handlers);
int freeHandlers(handler_t *handlers);

#endif
