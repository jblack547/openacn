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
#include <conio.h>
#include <time.h>
#include "stdafx.h"

#include "opt.h"
#include "acnstdtypes.h"
#include "acn_port.h"
#include "acnlog.h"

#include "epi10.h"
#include "mcast_util.h" /* handy multicast utilities */
#include "ntoa.h"
#include "component.h"
#include "slp.h"
#include "discover.h"
#include "sdtmem.h"
#include "sdt.h"
#include "msleep.h"

void process_keys(void);

bool recv_thread_terminate = FALSE;
bool tick_thread_terminate = FALSE;
WORD recv_thread_id;
WORD tick_thread_id;


/* Thread to receive data */
static DWORD WINAPI recv_funct(LPVOID lParam)
{
  while (!recv_thread_terminate) {
		/* netx_poll is semi-blocking. It only returns to give app time to shut things down. */
    netx_poll();
  }
  return 0;
}

/* Thread to deal with timer (100ms) intervals */
static DWORD WINAPI tick_funct(LPVOID lParam)
{
	/* call tick functions */
  while (!tick_thread_terminate) {
    sdt_tick(NULL);
    slp_tick(NULL);
    msleep(100);
  }
  return 0;
}

/*****************************************************************************************/
/* this section is modifed for each port */
char *dcid_str1 = "11111111-2222-3333-4444-aabbccddeeff";
char *cid_str1  = "11111111-2222-3333-4444-000000000000";
const char *myfctn = "ACN/MSVC";
const char *myuacn = "Unit 1";
/*****************************************************************************************/
/* this section is the same for each port*/
component_t *local_component = NULL;
component_t *foreign_component = NULL;

static void discover_callback(component_t *foreign_component)
{
  if (local_component && foreign_component) {
    /* we could auto join here... */
    /*sdt_join(local_component, foreign_component); */
  } else {
    printf("bad component\n");
  }
}
/* TODO: need to know if join succeeds or is disconnected. */

void process_keys(void)
{
  cid_t cid;
  cid_t dcid;
  char ip[50]   = "";
  char mask[50] = "";
  /* netx_addr_t   addr; */
  ip4addr_t     myip = 0;
  acn_protect_t protect;
  component_t  *comp;
  int           cnt;
  int           x;
  char          cid_text[37];
  sdt_member_t  *member;
  bool          WaitingForJoin = false;
  bool          WaitingForLeave = false;

  int		ch;
  bool	doexit = false;

  /* wait for ip address (in host format)*/
  while (!myip) {
    myip = ntohl(netx_getmyip(NULL));
  }
  
  /* make a local component */
  textToCid(cid_str1, cid);
  textToCid(dcid_str1, dcid);
  /* make our cid unique to us */
  cid[15] = (uint8_t)(myip & 0xff);
  /* create our local component */
  local_component = sdt_add_component(cid, dcid, true, accBOTH);
  
  /* fill in some required data */
  if (local_component) {
    /* local_component->callback = component_callback; */
    strcpy(local_component->uacn, myuacn);
    strcpy(local_component->fctn, myfctn);
  }

	/* main is debug thread */
	while (!doexit) {
		ch = _getch();
		if (ch == EOF) {
			break;
		}
 
    if (ch) {
      if (WaitingForJoin || WaitingForLeave) {
        if (ch < '1' || ch > '9') {
          WaitingForJoin = false;
          WaitingForLeave = false;
        }
      }
    }

		switch (ch) {
      case 0:
        /* do nothing */
        break;
			case 8: _putch(8); _putch(' '); _putch(8);
  			/* backspace  */
				break;
      case 10: _putch(10);
        break;
			case 13: _putch(10);
  			/* LF */
				break;
      case 'j': case 'J':
        printf("Enter component number to join: ");
        WaitingForJoin = true;
        break;
      case 'l': case 'L':
        printf("Enter component number to leave: ");
        WaitingForLeave = true;
        break;
      case '1': case '2': case '3':
        if (WaitingForJoin || WaitingForLeave) {
          x = ch - '0';
          protect = ACN_PORT_PROTECT();
          comp = sdt_first_component();
          cnt = 0;
          while (comp) {
            cnt++;
            if (cnt == x) {
              if (!comp->is_local) {
                if (WaitingForJoin) {
                  printf("Joining %d\n",cnt);
                  sdt_join(local_component, comp);
                }
                if (WaitingForLeave) {
                printf("Leaving %d\n",cnt);
                sdt_leave(local_component, comp);

                }

              } else {
                printf("Nope, %d is local!\n", cnt);
              }
            }
            comp = comp->next;
          }
          ACN_PORT_UNPROTECT(protect);
          WaitingForJoin = false;
          WaitingForLeave = false;
        }
        break;
			case 'd': case 'D':
        /* discover  */
		    discover_acn(dcid_str1, discover_callback);
				break;
			case 'i': case 'I': 
        /* We copy to local as ntoa can not be reused... */
        strcpy(ip, ntoa(netx_getmyip(0)));
        strcpy(mask, ntoa(netx_getmyipmask(0)));
			  printf("My IP: %s, mask %s\n", ip, mask);
        break;
      case 'c': case 'C':
        protect = ACN_PORT_PROTECT();
        comp = sdt_first_component();
        cnt = 0;
        while (comp) {
          cnt++;
          cidToText(comp->cid, cid_text);
          member = NULL;
          if (local_component->tx_channel) {
            member = sdt_find_member_by_component(local_component->tx_channel, comp);
          }
          printf("%d, %s %s\n", cnt, cid_text, comp->is_local?"(local)":(member?"(joined)":""));
          comp = comp->next;
        }
        ACN_PORT_UNPROTECT(protect);
        break;
			case 'r': case 'R': 
        discover_register(local_component);
				break;
      case 'u':case 'U':
        discover_deregister(local_component);
        break;
			case 's': case 'S':
				slp_stats();
        sdt_stats();
				break;
  		case 3: case 'x': case 'X':
	  		/* exit test app */
				doexit = true;
				break;
      case 't': case 'T':
        {
          #if 0
          rlp_txbuf_t *tx_buffer;
          uint8_t *buf_start;

          netx_INADDR(&addr) = DD2NIP(193,168,1,200);
          netx_PORT(&addr) = htons(6000);

          /* IP4_ADDR(&addr.sin_addr.S_un.S_addr, 192,168,1,200); */


          /* Create packet buffer*/
          tx_buffer = rlpm_new_txbuf(DEFAULT_MTU, local_component);
          if (!tx_buffer) {
            acnlog(LOG_ERR | LOG_SDT, "sdt_tx_join : failed to get new txbuf");
           break;
          }
          buf_start = rlp_init_block(tx_buffer, NULL);
          rlp_add_pdu(tx_buffer, buf_start, 49, NULL);
          
          rlp_send_block(tx_buffer, local_component->tx_channel->sock, &addr);
          rlpm_release_txbuf(tx_buffer);
          #endif
        }
        break;
      default:
		    if (ch >= ' ' && ch <= 126) {
			    _putch(ch);
		    }
		}
	}

  if (local_component) {
    sdt_del_component(local_component);
  }
  if (foreign_component) {
    sdt_del_component(foreign_component);
  }
  printf("Shutting down...\n");
}
/*****************************************************************************************/


int _tmain(int argc, _TCHAR* argv[])
{
HANDLE tick_thread;
HANDLE recv_thread;

  printf("Hello ACN World\n");

  /* note, this may not work in all cases. 
  1) all systems might boot at the same time
  2) when they get to this point, they may not have an ip yet
  */
  srand(GetTickCount() + netx_getmyip(0));

	/* init our acn stack */
  acn_port_protect_startup();
  netx_init();
  netx_startup();

  /* init/open modules */
  slp_init();
  slp_open();
  slp_active_discovery_start();
  rlp_init();
  sdt_init(); /* indirectly calls sdtm_init(), rlp_init(), rlpm_init() */
  sdt_startup(true);
  /* dmp_startup(); */

	/* create timer thread */
  tick_thread = CreateThread(NULL,0, tick_funct, NULL, CREATE_SUSPENDED, (LPDWORD)&tick_thread_id);
  SetThreadPriority(tick_thread,THREAD_PRIORITY_BELOW_NORMAL);

	/* create receive thread */
  recv_thread = CreateThread(NULL,0, recv_funct, NULL, CREATE_SUSPENDED, (LPDWORD)&recv_thread_id);
  SetThreadPriority(recv_thread,THREAD_PRIORITY_ABOVE_NORMAL);

  /* kick them off */
  ResumeThread(tick_thread);
  ResumeThread(recv_thread);

  /* go to our debugging terminal */
  process_keys();

  /* shut things down - these require the threads to continue run... */
  sdt_shutdown();
  slp_close();
  /* dmp_shutdown() */

  /* close threads */
  recv_thread_terminate = TRUE;
  WaitForSingleObject(recv_thread,INFINITE);
  tick_thread_terminate = TRUE;
  WaitForSingleObject(tick_thread,INFINITE);

  netx_shutdown();
  acn_port_protect_shutdown();

	slp_stats();
  sdt_stats();
  printf("========================\n");

  printf("Done...press any key to exit\n");
  _getch();
  printf("Bye bye..\n");
	return 0;
}

