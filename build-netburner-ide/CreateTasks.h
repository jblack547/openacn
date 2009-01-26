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

#ifndef _CREATE_TASKS_H
#define _CREATE_TASKS_H


// Priorities for tasks - 37 is highest for system
// Priorities can be from 63 (lowest) to 1 (highest)

#define ACN_PERIODIC_PRIORITY     (59) // this must be a low priority
#define ACN_RX_PRIORITY           (49) // ACN receive (E1.17)
#define DEBUG_TERM_PRIORITY       (61)

// OS system priorities are defined elsewhere constants.h
/*
#define MAIN_PRIO          (50)

#define HTTP_PRIO          (45)
#define PPP_PRIO           (44)
#define WIFI_TASK_PRIO     (41)
#define TCP_PRIO           (40)
#define IP_PRIO            (39)
#define ETHER_SEND_PRIO    (38)
#define SSH_TASK_PRIORITY  (56)
*/

// prototypes
int CreateTasks(void);

#endif //_CREATE_TASKS_H
