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

#include <startnet.h>
#include <stdio.h>

#include "CreateTasks.h"
#include "AcnPeriodicTasks.h"
#include "AcnRxTasks.h"

// Stacks for tasks
DWORD AcnPeriodicTasksStack[USER_TASK_STK_SIZE] __attribute__((aligned(4)));
DWORD AcnRxTaskStack[USER_TASK_STK_SIZE] __attribute__((aligned(4)));

OS_FLAGS tick_flag;         // Declare an OS_FLAGS object
OS_FLAGS recv_flag;         // Declare an OS_FLAGS object

int CreateTasks(void)
{
  int errCode;
  OSFlagCreate( &tick_flag ); // Initialize the object
  OSFlagCreate( &recv_flag ); // Initialize the object
  OSFlagSet(&tick_flag, 1);
  OSFlagSet(&recv_flag, 1);

  // Create the periodic processing task which does periodic SLP and SDT processing
  if ((errCode = OSTaskCreate(AcnPeriodicTasks,
                   (void *)&tick_flag,
                   (void *)&AcnPeriodicTasksStack[USER_TASK_STK_SIZE],
                   (void *)AcnPeriodicTasksStack,
                   ACN_PERIODIC_PRIORITY)) != OS_NO_ERR) {
    // Error creating AcnPeriodicTasks
    iprintf("Error Creating AcnPeriodicTasks = %d\n", errCode); // DEBUG
    return 0;
  }

  // Create the Rx processing task which receives UDP/IP messages
  if ((errCode = OSTaskCreate(AcnRxTask,
                   (void *)&recv_flag,
                   (void *)&AcnRxTaskStack[USER_TASK_STK_SIZE],
                   (void *)AcnRxTaskStack,
                   ACN_RX_PRIORITY)) != OS_NO_ERR) {
    // Error creating AcnRxTask
    iprintf("Error Creating AcnRxTask = %d\n", errCode); // DEBUG
    return 0;
  }
  return 1;
}
