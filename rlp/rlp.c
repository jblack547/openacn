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
//static const char *rcsid __attribute__ ((unused)) =
//   "$Id$";

/* acnlog facility DEBUG_RLP is used for ACN:RLP */

#include <string.h>
#include "opt.h"
#include "types.h"
#include "acn_arch.h"

#include "rlp.h"
#include "acn_rlp.h"

#include "rlpmem.h"
#include "acnlog.h"

#include "netsock.h"
#include "netxface.h"

#define NUM_PACKET_BUFFERS	16
#define BUFFER_ROLLOVER_MASK  (NUM_PACKET_BUFFERS - 1)

/* some handy macros */
#define LOG_FSTART() acnlog(LOG_DEBUG | LOG_RLP, "%s :...", __func__)


#if CONFIG_EPI17

static const uint8_t rlpPreamble[RLP_PREAMBLE_LENGTH] =
{
	0, RLP_PREAMBLE_LENGTH,		/* Network byte order */
	0, RLP_POSTAMBLE_LENGTH,	/* Network byte order */
	'A', 'S', 'C', '-', 'E', '1', '.', '1', '7', '\0', '\0', '\0'
};

#endif

/*
Min length structure for first PDU (no fields inherited) is:
	flags/length 2 octets
	vector (protocol ID) 4 octets
	header (source CID) 16 octets
	data variable - may be zero

in subsequent PDUs any field except flags/length may be inherited
*/

#define RLP_FIRSTPDU_MINLENGTH (2 + 4 + sizeof(cid_t))
#define RLP_PDU_MINLENGTH 2

/*

API description

This RLP implementation is designed for IPv4 - it may operate with IPv6
with little modification. This code understands IP but the stack details
are abstracted into netiface.c. This abstraction uses netsocket_s
structures representing each socket or similar UDP connection between
RLP and the stack. Each netsocket corresponds to a different local
unicast-address/port combination given by a netaddr_s.

Note - currently the only local unicast address supported is
NETI_GROUP_UNICAST - meaning any local address as decided by the network
stack. This may change.

RLP manages netsocket structures and the client calls rlp_open_netsock
to get one and rlp_close_netsock() to free it again.

To transmit PDUs the client only needs a netsocket which is passed to
rlp_send_block() once the PDU block has been assembled.

To receive packets, the client must call rlp_add_listener() to register
a callback function for a particular netsocket. A listener associates a
destination address for incoming packets (either a multicast group or
the unicast address associated with the netsocket), with a protocol
(e.g. SDT) and a callback function. When a packet is received matching
the netsocket, the destination address, and the protocol, the associated
callback function is invoked.

rlp_add_listener() subscribes to the supplied group address if
necessary.



Incoming packets
--
When adding a listener the client protocol provides a callback function
and reference pointer. On receipt of a PDU matching that listener
parameters, RLP calls the callback function passing the reference
pointer as a parameter. This  pointer can be used for any purpose by the
higher layer - it is treated as opaque  data by RLP. The callback is
also passed pointers to a structure (network layer dependent)
representing the transport address of the sender, and to the CID of the
remote component.

In order to manage the relationship between channels at the client
protocol side the network interface layer, RLP maintains three
structures:

	struct netsocket_s represents an interface to the netiface layer -
	there is one struct netsocket_s for each incoming (local) port and
	unicast address (but see multiple interface note above).

	struct rlp_rxgroup_s represents a multicast group (and port). Any
	struct netsocket_s may have multiple rxgroups. Because of stack
	vagaries with multicast handling, RLP examines and filters the
	multicast destination address of every incoming packet early on and
	finds the associated rlp_rxgroup_s If none is found, the packet is
	dropped. There is one rlp_rxgroup_s lookup per packet.

	struct rlp_listener_s represents a single callback to the client
	component. For each rxgroup there may be multiple filters. rxgroups
	are filtered for protocol (e.g. SDT) on a PDU by PDU basis. It is
	permitted to open multiple filters for the same parameters - the
	same PDU contents are passed to each in turn.

All parameters passed to the client protocol are passed by reference
(e.g. a pointer to the sender CID is passed, not the CID itself).  They
must be treated as constant data and not modified by the client.
However, they do not persist across calls so the client must copy any
items which it requires to keep.

*/

/************************************************************************/
/*

Management of structures in memory
--
To allow flexibility in implementation, the netsocket_s, rlp_rxgroup_s
and rlp_listener_s structures used are manipulated using the API of the
rlpmem module - they should not be accessed directly. This allows
dynamic or static allocation strategies, and organization by linked
lists, by arrays, or more complex structures such as trees. In minimal
implementations many of these calls may be overridden by macros for
efficiency.

The implementation of these calls is in rlpmem.c where alternative
static and dynamic implementations are provided.

*/
/************************************************************************/

/************************************************************************/
/*
Initialize RLP (if not already done)
*/
int
rlp_init(void)
{
	static bool initialized = 0;

  LOG_FSTART();

	if (!initialized)
	{
    /* initialize sub modules */
    nsk_netsocks_init();
    netx_init();
		rlpm_init();

		initialized = 1;
	}
	return 0;
}

/************************************************************************/
/*
  To send PDUs via the root layer, the client must fill in a RLP PDU
  block in an RLP txbuffer. It must first initialise the buffer by
  calling rlp_init_block() then each time it has completed a PDU it
  calls rlp_add_pdu(), finally it can call rlp_send_block().

  Both rlp_init_block() and rlp_add_pdu() return a pointer to a suitable
  location in the txbuffer to place the next PDU data. The client must
  not place data before this location in the txbuffer but may place it
  after or in different memory altogether. When rlp_add_pdu is called,
  the data is copied into the appropriate position in the txbuffer if
  necessary. However, if the suggested location was used for assembling
  the data, there is a very good chance that no further copying will be
  required.

for example.

	mybuf = rlpm_newtxbuf(200);
	pduptr = rlp_init_block(mybuf, NULL);
	... assemble first PDU data to pduptr ...
	pduptr = rlp_add_pdu(mybuf, pduptr, PDU1size, PROTO_XYZ, NULL);
	... assemble next PDU data to pduptr ...
	pduptr = rlp_add_pdu(mybuf, pduptr, PDU2size, PROTO_ABC, &savedDataPtr);
	rlp_send_block(mybuf);

*/
/************************************************************************/
/*
Start an RLP PDU block

  If a non-NULL pointer into the txbuffer is provided as datap, then the
  block is initialized to place the first PDU data in that position. The
  purpose of this is to facilitate re-sending PDUs.

  Following the example above, if we now want to resend the second PDU
  only (and we have retained mybuf) we can do:

	pduptr = rlp_init_block(mybuf, savedDataPtr);
	pduptr = rlp_add_pdu(mybuf, pduptr, PDU2size, PROTO_ABC, &savedDataPtr);
	rlp_send_block(mybuf);

*/
uint8_t *
rlp_init_block(struct rlp_txbuf_s *buf, uint8_t *datap)
{
	uint8_t *blockstart;

  LOG_FSTART();

	blockstart = buf->datap;

	if (datap != NULL)
	{
		uint8_t *trystart;

		if (datap > blockstart + buf->datasize) {
      acnlog(LOG_WARNING|LOG_RLP, "rlp_init_block: block overrun");
      return NULL;
    }

		trystart = datap - RLP_PREAMBLE_LENGTH - 2 - sizeof(protocolID_t) - sizeof(cid_t);

		if (trystart < blockstart) {
      acnlog(LOG_WARNING|LOG_RLP, "rlp_init_block: block underrun");
      return NULL;
    }

		blockstart = trystart;
	} else {
		datap = blockstart + RLP_PREAMBLE_LENGTH + 2 + sizeof(protocolID_t) + sizeof(cid_t);
	}

	buf->blockend = NULL;
	buf->blockstart = blockstart;
	return datap;
}

/************************************************************************/
/*
  Pack a PDU into a RLP block

  Vector
  --
  If CONFIG_RLP_SINGLE_CLIENT is set we suppress the protocol in all but
  the first PDU since it will never change. Otherwise unless
  CONFIG_RLP_OPTIMIZE_PACK is set we always fill in the vector.

  Header
  --
  We always suppress header (source CID) after the first PDU since this
  implementation forbids the highly esoteric case of packing PDUs from
  separate local components into the same packet.

  Data
  --
  Unless CONFIG_RLP_OPTIMIZE_PACK is set we don't bother to check for
  duplicate data and don't track where the last data was.

  Return value is suggested place for the next PDU data. We have to
  guess a good place based on assumptions of whether vector, header or
  data will be suppressed. In fact the only time we have to guess is
  when CONFIG_RLP_OPTIMIZE_PACK is set and we do not know whether the
  next PDU will be of the same protocol. At present we guess that it
  will be - if it isn't we will have to move the data up.

  If packetdatap is not NULL its target is filled in with the actual
  address of the data after packing.
*/

uint8_t *
rlp_add_pdu(
	struct rlp_txbuf_s *buf,
	uint8_t *pdudata,
	int size,
#if !CONFIG_RLP_SINGLE_CLIENT
	protocolID_t protocol,
#endif
	uint8_t **packetdatap
)
{
	uint16_t flags;
	uint8_t *pdup, *pduend;

  LOG_FSTART();

	pdup = buf->blockend;	/* end of last PDU if any */
	if (pdup == NULL)	{/* first PDU of block? */
		pdup = buf->blockstart + RLP_PREAMBLE_LENGTH + 2 + sizeof(protocolID_t) + sizeof(cid_t);
		flags = VECTOR_FLAG | HEADER_FLAG | DATA_FLAG;
	} else {
#if CONFIG_RLP_OPTIMIZE_PACK
		pdup += 2;	/* allow for length/flags field */
		flags = 0;
#if !CONFIG_RLP_SINGLE_CLIENT
		if (protocol != buf->protocol) {
			flags |= VECTOR_FLAG;
			pdup += sizeof(protocolID_t);
		}
#endif
		if (!(
				buf->curdatalen == size
				&& memcmp(pdudata, buf->curdata, size) == 0
			))
		{
			flags |= DATA_FLAG;
		}
#else /* not CONFIG_RLP_OPTIMIZE_PACK */
#if CONFIG_RLP_SINGLE_CLIENT
    /* since we only have SDT PDUs, we don't have to repeat vector (protocol id) */
		pdup += 2;	/* allow for length field */
		flags = DATA_FLAG;
#else
		pdup += 2 + sizeof(protocolID_t);
		flags = DATA_FLAG | VECTOR_FLAG;
#endif
#endif
	}

	/*
	  we haven't written anything yet (so we don't overwrite the PDU data)
	  but have set flags and pdup points to where the data is to go
	*/
#if CONFIG_RLP_OPTIMIZE_PACK
	if ((flags & DATA_FLAG)) {
		/* is data in the right place already? */
		if (pdudata != pdup) {
			/* move data into position */
			memmove(pdup, pdudata, size)
		}
		buf->curdata = pdup;
		buf->curdatalen = size;
		pdup += size;
	}
	if (packetdatap) {
    *packetdatap = buf->curdata;
  }
	pduend = pdup;
#else
	/* is data in the right place already? */
	if (pdudata != pdup) {
		/* move data into position */
		memmove(pdup, pdudata, size);
	}
	if (packetdatap) {
    *packetdatap = pdup;
  }
	pduend = pdup + size;
#endif

	/* now put the preamble (if first PDU) length and flags, vector and header in place */
	pdup = buf->blockend;
	if (pdup == NULL) {
		pdup = buf->blockstart;
		memcpy(pdup, rlpPreamble, RLP_PREAMBLE_LENGTH);
		pdup += RLP_PREAMBLE_LENGTH;
		/* add the source CID */
		marshalUUID(pdup + 2 + sizeof(protocolID_t), buf->owner->cid);
	}

	/* PDU length and flags */
	pdup = marshalU16(pdup, (uint16_t)(pduend - pdup) | flags);

	if ((flags & VECTOR_FLAG))	{
#if CONFIG_RLP_SINGLE_CLIENT
		marshalU32(pdup, CONFIG_RLP_SINGLE_CLIENT);
#else
		marshalU32(pdup, protocol);
		buf->protocol = protocol;
#endif
	}

	buf->blockend = pduend;
#if CONFIG_RLP_SINGLE_CLIENT || CONFIG_RLP_OPTIMIZE_PACK
	return pduend + 2;	/* guess next PDU will repeat protocol */
#else
	return pduend + 2 + sizeof(protocolID_t);	/* allow for new protocol */
#endif
}

/************************************************************************/
/*
  Send a formatted PDU block
*/

int
rlp_send_block(
	struct rlp_txbuf_s *buf,
	netxsocket_t      *netsock,
	const netx_addr_t *destaddr
)
{
  LOG_FSTART();

  return netx_send_to(netsock, destaddr, buf->netbuf, buf->blockend - buf->blockstart);

}

/************************************************************************/
/*
Find a matching netsocket or create a new one if necessary
*/

netxsocket_t *
rlp_open_netsocket(localaddr_t *localaddr)
{
	netxsocket_t *netsock;

  LOG_FSTART();

  /* see if a netsocket already exists for the given port */
	if (LCLAD_PORT(*localaddr) != netx_PORT_EPHEM && (netsock = nsk_find_netsock(localaddr))) {
	  return netsock;	/* found existing matching socket */
	}

  /* create a new netsocket structure for this port */
	if ((netsock = nsk_new_netsock()) == NULL) {
    acnlog(LOG_ERR|LOG_RLP, "rlp_open_netsocket: failed to create netsocket");
	  return NULL;		/* cannot allocate a new one */
	}

  /* open a UDP socket for this port */
	if (netx_udp_open(netsock, localaddr) != 0) {
		acnlog(LOG_ERR|LOG_RLP, "rlp_open_netsocket: failed to open netsocket");
		nsk_free_netsock(netsock);	/* UDP open fails */
		return NULL;
	}

  netsock->data_callback = rlp_process_packet;

  acnlog(LOG_DEBUG|LOG_RLP, "LOG_INFO|rlp_open_netsocket: port=%d", NSK_PORT(netsock));

	return netsock;
}

/************************************************************************/
void
rlp_close_netsocket(netxsocket_t *netsock)
{
  LOG_FSTART();

	if (rlpm_netsock_has_rxgroups(netsock)) {
    acnlog(LOG_WARNING|LOG_RLP,"rlp_close_netsocket: not closed, active groups");
    return;
  }

	netx_udp_close(netsock);
	nsk_free_netsock(netsock);
  acnlog(LOG_DEBUG|LOG_RLP,"rlp_close_netsocket: closed port=%d", NSK_PORT(netsock));
}

/************************************************************************/
/*
Find a matching rxgroup or create a new one if necessary
*/
static
struct rlp_rxgroup_s *
rlp_open_rxgroup(netxsocket_t *netsock, groupaddr_t groupaddr)
{
  struct rlp_rxgroup_s *rxgroup;

  LOG_FSTART();

	if (!is_multicast(groupaddr) && groupaddr != netx_GROUP_UNICAST)
	{
		/* a specific non-multicast address has been provided - dangerous */
		/* FIXME should we check for broadcast here? Otherwise it is dropped */
		/*
		FIXME it is hard to check against all valid unicast addresses
		(loopback etc). Should we look through the routing table here? Is
		there a way to get the stack to check?
		For now just assume a single valid address.
		*/
		if (groupaddr != netx_getmyip(netx_INADDR_ANY)) {
      acnlog(LOG_WARNING|LOG_RLP, "LOG_DEBUG|rlp_open_rxgroup: illegal address");
      return NULL;	/* illegal address */
    }
		groupaddr = netx_GROUP_UNICAST;		/* force generic unicast */
	}

    // see if this entry already exists
	if ((rxgroup = rlpm_find_rxgroup(netsock, groupaddr))) return rxgroup;	/* found existing matching group */

	// create a new entry in the listeners[] table and get the pointer to the new entry
	if ((rxgroup = rlpm_new_rxgroup(netsock, groupaddr)) == NULL) {
    acnlog(LOG_ERR|LOG_RLP, "LOG_DEBUG|rlp_open_rxgroup: failed to create listener");
    return NULL;	/* cannot allocate a new one */
  }

	if (groupaddr != netx_GROUP_UNICAST) {
		if (netx_change_group(netsock, groupaddr, netx_JOINGROUP) != 0) {
      acnlog(LOG_ERR|LOG_RLP, "LOG_DEBUG|rlp_open_rxgroup: failed to join multicast address");
			rlpm_free_rxgroup(netsock, rxgroup);
			return NULL;
		}
	}
	return rxgroup;
}


/************************************************************************/
static
void
rlp_close_rxgroup(netxsocket_t *netsock, struct rlp_rxgroup_s *rxgroup)
{
  LOG_FSTART();

	if (rlpm_rxgroup_has_listeners(rxgroup)) {
    acnlog(LOG_WARNING|LOG_RLP, "LOG_DEBUG|rlp_close_rxgroup: can't close, has listeners");
    return;
  }

	if (rxgroup->groupaddr != netx_GROUP_UNICAST) {
	  netx_change_group(netsock, rxgroup->groupaddr, netx_LEAVEGROUP);
  }
	rlpm_free_rxgroup(netsock, rxgroup);
}

/************************************************************************/
/*
Add a new listener for a protocol, port number given in netsock, and multicast IP or 0 for unicast
*/
struct rlp_listener_s *
rlp_add_listener(netxsocket_t *netsock, groupaddr_t groupaddr, protocolID_t protocol, rlpHandler_t *callback, void *ref)
{
	struct rlp_listener_s *listener;
	struct rlp_rxgroup_s *rxgroup;

  LOG_FSTART();

   /* get a new element in the listeners[] table */
	if ((rxgroup = rlp_open_rxgroup(netsock, groupaddr)) == NULL) {
    acnlog(LOG_ERR|LOG_RLP, "LOG_DEBUG|rlp_add_listener: failed to open listener group");
		return NULL;
	}

  /* get address of a new listener entry */
	if ((listener = rlpm_new_listener(rxgroup)) == NULL) {
    acnlog(LOG_ERR|LOG_RLP, "LOG_DEBUG|rlp_add_listener: failed get new listener");
		rlp_close_rxgroup(netsock, rxgroup);
		return NULL;
	}

    // save data for this listener
	listener->protocol = protocol;
	listener->callback = callback;
	listener->ref = ref;

	return listener;
}

void
rlp_del_listener(netxsocket_t *netsock, struct rlp_listener_s *listener)
{
	struct rlp_rxgroup_s *rxgroup;

  LOG_FSTART();

  /* save the group */
  rxgroup = listener->rxgroup;

  /* free the listener */
	rlpm_free_listener(listener);

	/* close and free the group if now empty */
	rlp_close_rxgroup(netsock, rxgroup);
}

/************************************************************************/
/*
Process a packet - called by network interface layer on receipt of a packet
NOTE: The dest parameter is the IP address of the interface that received the packect. In the case of multicast,
      this would not be the same destination address as the ethernet packet.
*/
void
rlp_process_packet(netxsocket_t *socket, const uint8_t *data, int length, netx_addr_t *dest, netx_addr_t *source, void *ref)
{
	struct rlp_rxgroup_s *rxgroup;
	struct rlp_listener_s *listener;
	const uint8_t *src_cidp;
	protocolID_t pduProtocol;
	const uint8_t *pdup, *datap = NULL;
	int datasize = 0;

  LOG_FSTART();

	pdup = data;
	if(length < (int)(RLP_PREAMBLE_LENGTH + RLP_FIRSTPDU_MINLENGTH + RLP_POSTAMBLE_LENGTH)) {
		acnlog(LOG_DEBUG|LOG_RLP,"rlp_process_packet: Packet too short to be valid");
		return;
	}

	/* Check and strip off EPI 17 preamble  */
	if(memcmp(pdup, rlpPreamble, RLP_PREAMBLE_LENGTH)) {
		acnlog(LOG_DEBUG|LOG_RLP,"rlp_process_packet: Invalid Preamble");
		return;
	}
	pdup += RLP_PREAMBLE_LENGTH;

	/* Find if we have a handler */
	rxgroup = rlpm_find_rxgroup(socket, netx_INADDR(dest));
	if (rxgroup == NULL) {
    acnlog(LOG_DEBUG|LOG_RLP,"rlp_process_packet: No handler for this dest address");
	  return;	/* No handler for this dest address */
	}

	src_cidp = NULL;
	pduProtocol = PROTO_NONE;

	/* first PDU must have all fields */
	if ((*pdup & (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG | LENGTH_bFLAG)) != (VECTOR_bFLAG | HEADER_bFLAG | DATA_bFLAG)) {
		acnlog(LOG_DEBUG|LOG_RLP,"rlp_process_packet: illegal first PDU flags");
		return;
	}

	/* pdup points to start of PDU */
	while (pdup < data + length)
	{
		uint8_t flags;
		const uint8_t *pp;

		flags = *pdup;
		pp = pdup + 2;
		pdup += getpdulen(pdup);	/* pdup now points to end */
		if (pdup > data + length)	{ /* sanity check */
		  acnlog(LOG_DEBUG|LOG_RLP,"rlp_process_packet: packet length error");
			return;
		}
		if (flags & VECTOR_bFLAG) {
			pduProtocol = unmarshalU32(pp); // get protocol type
			pp += sizeof(uint32_t);
		}
		if (flags & HEADER_bFLAG) {
			src_cidp = pp; // get pointer to source CID
			pp += sizeof(cid_t);
		}
		if (pp > pdup) {// if there is no following PDU in the message
			acnlog(LOG_DEBUG|LOG_RLP, "rlp_process_packet: pdu length error");
			return;
		}
		if (flags & DATA_bFLAG)	{
			datap = pp; // get pointer to start of the PDU
			datasize = pdup - pp; // get size of the PDU
		}
		/* there may be multiple channels registered for this PDU */
		for (
			listener = rlpm_first_listener(rxgroup, pduProtocol);
			listener != NULL;
			listener = rlpm_next_listener(listener, pduProtocol)
		)
		{
			if (listener->callback) {
        // TODO: hack.. well sort of, clipping out lister->ref and sending back ref from call back
        // on lwip, this is pointer to pbuf so we can do reference counting on it
//				(*listener->callback)(datap, datasize, listener->ref, remhost, src_cidp);
				(*listener->callback)(datap, datasize, ref, source, src_cidp);
      }
		}
	}
}

