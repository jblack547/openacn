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
*/
#include <stdio.h>
#include <startnet.h>
#include <stdlib.h>

#include "netinterface.h"
#include "constants.h"

#include "slp.h"
#include "sdt.h"
#include "dmp.h"
#include "AcnPeriodicTasks.h"
#include "ntoa.h"

#if CONFIG_SDT
void component_callback(component_event_t state, void *param1, void *param2) {
  iprintf("Callback: %d leader: %x, member %x\n ", state, (int)param1, (int)param2);
}
#endif

// The passed in parm "pd" is flag to indicate to shut down...
void AcnPeriodicTasks(void *pd)
{
  OS_FLAGS *task_flag;
  task_flag = (OS_FLAGS*)pd;

  int    inf;

  /* wait here till we have an IP address */
  // TODO: this need to be made so IP can come (and go) later
  iprintf("Waiting for interface...");
  inf = GetFirstInterface();


  while (OSFlagState(task_flag)) {
    if (InterfaceIP(inf) != 0) {
      break;
    }
    GetIfConfig(inf);
    iprintf(".");
  }

    /* display our IP */
  iprintf("\nGot IP:%s\n", ntoa(InterfaceIP(inf)));

  /* loop forever in this task */
  while (OSFlagState(task_flag)) {
    /* do SLP processing */
#if CONFIG_SLP
    slp_tick(0);
#endif

    /* do SDT processing */
#if CONFIG_SDT
    sdt_tick(0);
#endif

    /* do DMP processing */
#if CONFIG_DMP
    //dmp_tick();
#endif

    /* delay 100mS */
    OSTimeDly(TICKS_PER_SECOND/10);
  }
  OSFlagSet(task_flag, 1);
  OSTaskDelete();
}

