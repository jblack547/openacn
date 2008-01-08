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

/* 

Implementation of a client protocol handler list for SDT

*/ 
#include "sdt_handlers.h"
#include "opt.h"
#include <syslog.h>
#include <stdlib.h>

#ifdef CONFIG_SDTMEM_STATIC

#define SDT_MAX_HANDLERS 10
static struct sdt_handler_s handlers[SDT_MAX_HANDLERS];

/***********************************************************************************************/
void
sdt_init_handlers(void)
{
	struct sdt_handler_s *h;
	for(h = handlers; h < handlers + SDT_MAX_HANDLERS; ++h){
		h->protocol = PROTO_NONE;
		h->rx_handler = NULL;
		h->msg_handler = NULL;
		h->ref = NULL;
	}
}

/***********************************************************************************************/
/*
  find the first handler for protocol p 
*/
static struct sdt_handler_s *
_find_handler(protocolID_t protocol)
{
	struct sdt_handler_s *h;
	h = handlers;
	while (h->protocol != protocol)
		if(++h >= handlers + SDT_MAX_HANDLERS) return (NULL);
	return h;
}


/***********************************************************************************************/
/*
  Register handlers with SDT
  Returns 0 if all is OK 
  TODO: It is an error/warning if the protocol has already been registered. 
*/
int 
sdt_register_handlers(
		      protocolID_t p, 
		      rx_handler_t *rx_handler, 
		      msg_handler_t *msg_handler, 
		      void *ref
		      )
{
	struct sdt_handler_s *h;
	if(_find_handler(p)){
        	syslog(LOG_ERR|LOG_LOCAL1,"sdt_register_handlers: Handler already registered. Deregister first.");
		return(1); /* TODO decent sane error handling */	
	}

	h = _find_handler(PROTO_NONE); /* First empty slot. */
	if(!h)
		{
			syslog(LOG_ERR|LOG_LOCAL1,"sdt_register_handlers: Handler cannot be registered.");
			return(2); /* TODO error handling */
		}
	h->protocol = p;
	h->rx_handler = rx_handler;
	h->msg_handler = msg_handler;
	h->ref = ref;
	return 0;
}

/***********************************************************************************************/
/* De-register the handler for protocol p and remove it from the list 
*/
int 
sdt_deregister_handlers(protocolID_t p)
{
	struct sdt_handler_s *h;
	h = _find_handler(PROTO_NONE); /* First empty slot. */
	if(!h)
		{
			syslog(LOG_ERR|LOG_LOCAL1,"sdt_deregister_handlers: Handler cannot be deregistered.");
			return(3); 
		}
	h->protocol = PROTO_NONE;
	h->rx_handler = NULL;
	h->msg_handler = NULL;
	h->ref = NULL;
	return 0;
}

/***********************************************************************************************/
/* Returns the RX handler for the protocol p. The reference pointer is loaded into ref.
 */ 
rx_handler_t *
sdt_get_rx_handler(protocolID_t p, void *ref)
{
	struct sdt_handler_s *h;
	h = _find_handler(p); 
	ref = h->ref;
	return h->rx_handler;
}

/***********************************************************************************************/
/* Returns the message handler for the protocol p. The reference pointer is loaded into ref.
 */ 
msg_handler_t * 
sdt_get_msg_handler(protocolID_t p, void *ref)
{
	struct sdt_handler_s *h;
	h = _find_handler(p); 
	ref = h->ref;
	return h->msg_handler;
}


/***********************************************************************************************/
/* Does nothing in the static memory case. 
 */ 
void
sdt_free_handlers(void)
{
}

#elif CONFIG_SDTMEM_MALLOC

/***********************************************************************************************/
/*
  For systems with dynamic memory allocation. 
  A linked list to handlers registered by higher level protocols. 
*/
static struct sdt_handler_s *handlers;

/***********************************************************************************************/
static struct sdt_handler_s *
_new_handler()
{
	struct sdt_handler_s *h =  malloc(sizeof(struct sdt_handler_s));
	if(!h){
		return NULL;
	}
	h->protocol = PROTO_NONE;
	h->rx_handler = NULL;
	h->msg_handler = NULL;
	h->ref = NULL;
	h->prev = h;
	h->next = h; /* link to self */
	return h;
}

/***********************************************************************************************/
void 
_insert_handler(struct sdt_handler_s *h)
{	
	/* insert into the list */
	h->prev = handlers;
	h->next = handlers->next;
	handlers->next = h;
	h->next->prev = h;
}


/***********************************************************************************************/
/* 
   Deletes a handler from the list and returns a pointer to the next handler.
*/
static struct sdt_handler_s *
_delete_handler(struct sdt_handler_s *h)
{
	struct sdt_handler_s *next;
	if(h->next == h){ /* list has 1 member */
		free(h);
		return NULL;
	}
	
        next = h->next;
	if(h->prev)
		h->prev->next = next;
	if(next)
		next->prev = h->prev;
	free(h);
	return next;
}

/***********************************************************************************************/
/* Returns the first handler for protocol p 
   Returns NULL if the handler is not in the list 
*/

static struct sdt_handler_s *
_find_handler(protocolID_t p){
	struct sdt_handler_s *h = handlers;
	
	while(h->protocol != p){
		if (h->next == handlers)
			return NULL;
		h = h->next;
	}
	return h;
}

/***********************************************************************************************/

void
sdt_init_handlers(void)
{
	handlers = _new_handler();
	if(!handlers){
		syslog(LOG_ERR|LOG_LOCAL1,"init_sdt_handlers: Out of memory. Cannot allocate handlers.");
		return;
	}
	handlers->next = handlers; /* point to self */
	handlers->prev = handlers; /* point to self */
	handlers->protocol = PROTO_NONE;
	handlers->rx_handler = NULL;
	handlers->msg_handler = NULL;
	return;
}


/***********************************************************************************************/
/*
  
*/
rx_handler_t *
sdt_get_rx_handler(protocolID_t p, void* ref)
{
	struct sdt_handler_s *h;
	h = _find_handler(p);
	if(!h)	return NULL;
	ref = h->ref;
	return h->rx_handler;

}


/***********************************************************************************************/
/*

*/
msg_handler_t *
sdt_get_msg_handler(protocolID_t p, void* ref)
{
	struct sdt_handler_s *h;
	h = _find_handler(p);
	if(!h) return NULL;
	ref = h->ref;
	return h->msg_handler;
}


/***********************************************************************************************/
/*
  Register a client protocol's message handler packet RX handler with SDT
*/
int 
sdt_register_handlers(
		      protocolID_t p, 
		      rx_handler_t *rx_handler, 
		      msg_handler_t *msg_handler, 
		      void *ref
		      )
{

	struct sdt_handler_s *h = _new_handler(); 

	if(!h){
		syslog(LOG_ERR|LOG_LOCAL1,"sdtRegisterHandlers: Cannot allocate memory for new handler.");
		return(-1);
	} 
	_insert_handler(h);
	h->protocol = p;
	h->rx_handler = rx_handler;
	h->msg_handler = msg_handler;
	h->ref = ref;
	return 0;
}


/***********************************************************************************************/
/*
  Removes a handler from the list.
*/
int 
sdt_deregister_handlers(protocolID_t p)
{
	struct sdt_handler_s *h;
	h = _find_handler(p);
	
	if(!h){
		syslog(LOG_ERR|LOG_LOCAL1,"sdt_deregister_handlers, Handler for protocol %d not found.",(int)p);
		return 1; 
	}
	_delete_handler(h);
	return 0;
}


/***********************************************************************************************/
/*
  Get rid of our handler list
*/

void
sdt_free_handlers(void)
{
	while(handlers){ 
		handlers = _delete_handler(handlers);
	}
}

#endif /* CONFIG_SDTMEM_MALLOC */


