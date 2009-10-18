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

  $Id: acntest.c 269 2009-01-26 07:20:12Z bflorac $
*/

#include <stdio.h>
#include <stdlib.h>

#include <termios.h>
#include <unistd.h>
#include <pthread.h>    /* POSIX Threads */

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

#include <sys/time.h>
long microsecsonds_since_midnight_GMT(void)
{
  struct timeval tvs;
  gettimeofday(&tvs, NULL);
  return tvs.tv_usec;
}

void process_keys(void);

int mygetch(void)
{
  struct termios oldt, newt; 
  int ch;
  
  tcgetattr( STDIN_FILENO, &oldt );
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr( STDIN_FILENO, TCSANOW, &newt );
  ch = getchar();
  tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
  return ch;
}


pthread_t timer_thread_id;
pthread_t recv_thread_id;

/* Thread to receive data */
void *recv_funct(void *lParam)
{
 (void)(lParam);
  printf("HELLO from recv_funct\n");

  while (1) {
    /* test to see if we should cancel */
    pthread_testcancel();
    /* netx_poll is semi-blocking. It only returns to give app time to shut things down. */
    netx_poll();
  }
}

/* Thread to deal with timer (100ms) intervals */
void *timer_funct(void *lParam)
{
  (void)(lParam);
  printf("HELLO from timer_funct\n");

  /* call tick functions */
  while (1) {
    /* test to see if we should cancel */
    pthread_testcancel();
    slp_tick(NULL);
    sdt_tick(NULL);
    msleep(100); /* 100ms */
  }
}


#define _getch()  mygetch()
#define _putch(x) putchar(x)

/*****************************************************************************************/
/* this section is modifed for each port */
char *dcid_str1 = "11111111-2222-3333-4444-aabbccddeeff";
char *cid_str1  = "11111111-2222-3333-4444-000000000000";
const char *myfctn = "ACN/BSD";
const char *myuacn = "Unit 1";
/*****************************************************************************************/
/* this section is the same for each port*/
component_t *local_component = NULL;
component_t *foreign_component = NULL;

static void discover_callback(component_t *foreign_component)
{
  PRINTF("%s\n","joining new device");
  if (local_component && foreign_component) {
    /* sdt_join(local_component, foreign_component); */
  } else {
    PRINTF("%s\n","bad component");
  }
}
/* TODO: need to know if join succeeds or is disconnected. */

void process_keys(void)
{
#if CONFIG_NSK
  char ip[50]   = "";
  char mask[50] = "";
  /* netx_addr_t   addr; */
  ip4addr_t     myip = 0;
#endif

  acn_protect_t protect;
  component_t  *comp;
  int           cnt;
  int           x;
  char          cid_text[37];

#if CONFIG_SDT
  cid_t cid;
  cid_t dcid;
  sdt_member_t  *member;
#endif

  bool          WaitingForJoin = false;
  bool          WaitingForLeave = false;

  int   ch;
  bool  doexit = false;

  /* wait for ip address (in host format)*/
#if CONFIG_NSK
  while (!myip) {
    myip = ntohl(netx_getmyip(NULL));
  }
#endif

#if CONFIG_SDT
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
#endif
  /*  main is debug thread */
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
        /* backspace */
        break;
      case 10: _putch(10);
        break;
      case 13: _putch(10);
        /* lf */
        break;
      case 'j': case 'J':
        PRINTF("%s","Enter component number to join: ");
        WaitingForJoin = true;
        break;
      case 'l': case 'L':
        PRINTF("%s","Enter component number to leave: ");
        WaitingForLeave = true;
        break;
#if CONFIG_SDT
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
                  PRINTF("Joining %d\n",cnt);
                  sdt_join(local_component, comp);
                }
                if (WaitingForLeave) {
                  PRINTF("Leaving %d\n",cnt);
                sdt_leave(local_component, comp);

                }

              } else {
                PRINTF("Nope, %d is local!\n", cnt);
              }
            }
            comp = comp->next;
          }
          ACN_PORT_UNPROTECT(protect);
          WaitingForJoin = false;
          WaitingForLeave = false;
        } else {
          PRINTF("%s",".\n");
        }
        break;
#endif
#if CONFIG_SLP
      case 'd': case 'D':
        /*  discover */
        discover_acn(dcid_str1, discover_callback);
        break;
#endif
#if CONFIG_NSK
      case 'i': case 'I':
        /* We copy to local as ntoa can not be reused... */
        strcpy(ip, ntoa(netx_getmyip(0)));
        strcpy(mask, ntoa(netx_getmyipmask(0)));
        PRINTF("My IP: %s, mask %s\n", ip, mask);
        break;
#endif
#if CONFIG_SDT
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
          PRINTF("%d, %s %s\n", cnt, cid_text, comp->is_local?"(local)":(member?"(joined)":""));
          comp = comp->next;
        }
        ACN_PORT_UNPROTECT(protect);
        break;
#endif
#if CONFIG_SLP
      case 'r': case 'R':
        discover_register(local_component);
        break;
      case 'u':case 'U':
        discover_deregister(local_component);
        break;
#endif
#if CONFIG_SDT
      case 's': case 'S':
        slp_stats();
        sdt_stats();
        break;
#endif
      case 3: case 'x': case 'X':
        /* exit test app */
        doexit = true;
        break;
      case 't': case 'T':
#if 0
        {
          rlp_txbuf_t *tx_buffer;
          uint8_t *buf_start;
          /* uint8_t *buffer; */

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
        }
        break;
#endif
      default:
        if (ch >= ' ' && ch <= 126) {
          _putch(ch);
        }
    }
  }

#if CONFIG_SDT
  if (local_component) {
    sdt_del_component(local_component);
  }
  if (foreign_component) {
    sdt_del_component(foreign_component);
  }
#endif
  PRINTF("%s","Shutting down...\n");
}
/*****************************************************************************************/

int main()
{
  PRINTF("%s","Hello ACN World\n");

   /* note, this may not work in all cases. 
   1) all systems might boot at the same time
   2) when they get to this point, they may not have an ip yet
   */

  /* init our acn stack */
  acn_port_protect_startup();
#if CONFIG_NSK
  srand(microsecsonds_since_midnight_GMT() + netx_getmyip(0));
  netx_init();
  netx_startup();
#endif

#if CONFIG_SLP
  slp_init();
  slp_open();
  slp_active_discovery_start();
#endif

#if CONFIG_RLP
  rlp_init();
#if CONFIG_SDT
  sdt_init();
  sdt_startup(true);
#endif
  /* dmp_startup(); */ 
#endif /* RLP */

  pthread_create(&timer_thread_id, NULL, timer_funct, NULL);
  pthread_create(&recv_thread_id, NULL, recv_funct, NULL);

  process_keys();

  /* shut things down - these require the threads to continue run... */
#if CONFIG_SDT
  sdt_shutdown();
#endif
#if CONFIG_SLP
  slp_close();  
#endif
  /* dmp_shutdown() */

  /* close threads */
  pthread_cancel(recv_thread_id);
  pthread_join(recv_thread_id, NULL);
  pthread_cancel(timer_thread_id);
  pthread_join(timer_thread_id, NULL);

#if CONFIG_NSK
  netx_shutdown();
#endif
  acn_port_protect_shutdown();

  slp_stats();
  sdt_stats();
  PRINTF("%s","========================\n");

  PRINTF("%s","Done...\n");
  return 0;
}

