/*--------------------------------------------------------------------*/
/*

Copyright (c) 2007, Engineering Arts (UK)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Engineering Arts nor the names of its
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
/************************************************************************/
/*
DUMMY C File

This file is only preprocessed - not fully compiled, and renders
relevant configuration options into a form suitable for make
*/

#include "opt.h"

/* Processor */
#if ARCH_x86_64
ARCH:=x86_64
#elif ARCH_i386
ARCH:=i386
#elif ARCH_h8300
ARCH:=h8300
#elif ARCH_h8300s
ARCH:=h8300s
#elif ARCH_thumb
ARCH:=thumb
#elif ARCH_arm
ARCH:=arm
#elif ARCH_coldfire
ARCH:=coldfire
#elif ARCH_m68k
ARCH:=m68k
#endif 

/* System/stack */
#if CONFIG_STACK_BSD
SYS:=bsd
#elif CONFIG_STACK_WIN32
SYS:=win32
#elif CONFIG_STACK_LWIP
SYS:=lwip
#elif CONFIG_STACK_PATHWAY
SYS:=pathway
#elif CONFIG_STACK_NETBURNER
SYS:=netburner
#endif 

/* Subsystems */
#if CONFIG_RLP
ACNPARTS+=rlp
#endif
#if CONFIG_SDT
ACNPARTS+=sdt
#endif
#if CONFIG_DMP
ACNPARTS+=dmp
#endif
#if CONFIG_SLP
ACNPARTS+=slp
#endif
#if CONFIG_E131
ACNPARTS+=e131
#endif
