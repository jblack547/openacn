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

	$Id: acn_config.h 2 2007-09-17 09:31:30Z philipnye $

*/
/*--------------------------------------------------------------------*/
/*
Information structures and handling relating to components
*/
#ifndef __component_h__
#define __component_h__ 1

#include "types.h"
#include "uuid.h"

/************************************************************************/
/*
  Local component structure is a rag-bag of information which represents
  a component within the local host. For efficiency, each layer can
  include it's own information within the component structure. because
  it is subject to many config options, other layers cannot rely on
  information outside their own scope and should leave it alone.

  Comonents must maintain their component structure in a fixed location
  as pointers to it may be stored and dereferenced at a later time.
*/

typedef struct local_component_s
{
	cid_t cid;
#if CONFIG_EPI10
	uint16_t dyn_mcast;
#endif
} local_component_t;

#if CONFIG_SINGLE_COMPONENT
extern local_component_t the_component;
#endif

#endif
