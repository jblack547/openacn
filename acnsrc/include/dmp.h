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
#ifndef __dmp_h__
#define __dmp_h__ 1

#include "opt.h"
#include "acn_arch.h"
#include "acnstdtypes.h"
#include "component.h"
#include "acn_dmp.h"
/* #include "sdt.h" */

#ifdef __cplusplus
extern "C" {
#endif

/* Address/Data Encoded byte fields (header) */
#define VIRTUAL_ADDRESS_BIT      0x80
#define RELATIVE_ADDRESS_BIT     0x40
#define ADDRESS_TYPE_MASK        0x30
#define ADDRESS_SIZE_MASK        0x03
#define RESERVED_BIT_MASK        0x0C

#define DMP_VECTOR_LEN 1
#define DMP_HEADER_LEN 1

/* DMP reliable flag */
enum
{
	NOT_RELIABLE,
	RELIABLE
};

/* address_size field */
enum
{
  ONE_OCTET_ADDRESS  = 0,
  TWO_OCTET_ADDRESS  = 1,
  FOUR_OCTET_ADDRESS = 2,
  RESERVED           = 3,
};

/* address_type field */
enum
{
  SINGLE_ADDRESS_SINGLE_DATA    = 0x00,
  RANGE_ADDRESS_SINGLE_DATA     = 0x10,
  RANGE_ADDRESS_EQUAL_SIZE_DATA = 0x20,
  RANGE_ADDRESS_MIXED_SIZE_DATA = 0x30,
};

/* Reason codes [DMP spec] */
enum 
{
  DMP_REASON_SUCCESS          = 0,
  DMP_REASON_NONSPEC          = 1,
  DMP_NOT_A_PROPERTY          = 2,
  DMP_WRITE_ONLY              = 3,
  DMP_NOT_WRITABLE            = 4,
  DMP_DATA_ERROR              = 5,
  DMP_MAPS_NOT_SUPPORTED      = 6,
  DMP_SPACE_NOT_AVAILABLE     = 7,
  DMP_PROP_NOT_MAPABLE        = 8,
  DMP_MAP_NOT_ALLOCATED       = 9,
  DMP_SUBSCRIPT_NOT_SUPPORTED = 10,
  DMP_NO_SUBSCRIPT_SUPPORTED  = 11,
};

/* DMP messsage types (commands) */
enum
{
  DMP_GET_PROPERTY =          1,
  DMP_SET_PROPERTY =          2,
  DMP_GET_PROPERTY_REPLY =    3,
  DMP_EVENT =                 4,
  DMP_MAP_PROPERTY =          5,
  DMP_UNMAP_PROPERTY =        6,
  DMP_SUBSCRIBE =             7,
  DMP_UNSUBSCRIBE =           8,
  DMP_GET_PROPERTY_FAIL =     9,
  DMP_SET_PROPERTY_FAIL =     10,
  DMP_MAP_PROPERTY_FAIL =     11,
  DMP_SUBSCRIBE_ACCEPT =      12,
  DMP_SUBSCRIBE_REJECT =      13,
  DMP_ALLOCATE_MAP =          14,
  DMP_ALLOCATE_MAP_REPLY =    15,
  DMP_DEALLOCATE_MAP =        16,
};

typedef enum {
  DMP_SUB_EMPTY,
  DMP_SUB_PENDING,
  DMP_SUB_ACCEPTED,
  DMP_SUB_FAIL
} dmp_subscription_state_t;  


typedef struct dmp_address_s
{
	uint32_t address_type; /* address type; SINGLE_ADDRESS_SINGLE_DATA... */
  uint32_t address_size; /* enumeration value: ONE_OCTET_ADDRESS...*/
	uint32_t address_start;
	uint32_t address_inc;
  uint32_t num_props;
	bool     is_single_value;  /* else it is a range */
	bool     is_virtual;       /* else it is Absolute */
} dmp_address_t;

typedef struct dmp_property_s {
  uint32_t        address;
  uint32_t        ref_count;
  /* dmp_property_t  *next; */
} dmp_property_t;

typedef struct dmp_subscription_s {
  dmp_property_t             *property;
  dmp_subscription_state_t    state;
  uint32_t                    expires_ms;            
  struct dmp_subscription_s  *next;
} dmp_subscription_t;


int      dmp_init(void);
void     dmp_client_rx_handler(component_t *local_component, component_t *foreign_component, bool is_reliable, const uint8_t *data, uint32_t data_len, void *ref);
void     dmp_tx_pdu(component_t *local_component, component_t *foreign_component, bool is_reliable, uint8_t *datap, int data_len);

void     dmp_tx_get_prop_reply(component_t *local_component, component_t *foreign_component, dmp_address_t *address, uint8_t *ptrProperty, uint16_t sizeofProperty);
void     dmp_tx_get_prop_fail(component_t *local_component, component_t *foreign_component, dmp_address_t *address, int8_t result);
void     dmp_tx_set_prop_fail(component_t *local_component, component_t *foreign_component, dmp_address_t *address, int8_t result);
void     dmp_tx_subscribe_accept(component_t *local_component, component_t *foreign_component, dmp_address_t *address);
void     dmp_tx_subscribe_reject(component_t *local_component, component_t *foreign_component, dmp_address_t *address, int8_t result);
void     dmp_tx_event(component_t *local_component, component_t *foreign_component, dmp_address_t *address, uint8_t *ptrProperty, uint16_t sizeofProperty);

int      dmp_decode_address_header(uint8_t address_type, uint8_t *data, dmp_address_t *dmp_address);
uint8_t* dmp_encode_address_header(dmp_address_t *dmp_address, uint8_t *encodeByte, uint8_t *datap);


#ifdef __cplusplus
}
#endif

#endif /* __dmp_h__ */
