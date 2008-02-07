/*--------------------------------------------------------------------*/
/*

Copyright (c) 2008, Electronic Theatre Controls, Inc

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

	$Id: $

*/
/*--------------------------------------------------------------------*/

#ifndef __acnlog_h
#define __acnlog_h

// wrf - borrowed from FreeBSD  
/* facility codes */
#define LOG_KERN        (0<<3)  /* kernel messages */
#define LOG_USER        (1<<3)  /* random user-level messages */
#define LOG_MAIL        (2<<3)  /* mail system */
#define LOG_DAEMON      (3<<3)  /* system daemons */
#define LOG_AUTH        (4<<3)  /* authorization messages */
#define LOG_SYSLOG      (5<<3)  /* messages generated internally by syslog */
#define LOG_LPR         (6<<3)  /* line printer subsystem */
#define LOG_NEWS        (7<<3)  /* network news subsystem */
#define LOG_UUCP        (8<<3)  /* UUCP subsystem */
#define LOG_CRON        (9<<3)  /* clock daemon */
#define LOG_AUTHPRIV    (10<<3) /* authorization messages (private) */
                                /* Facility #10 clashes in DEC UNIX, where */
                                /* it's defined as LOG_MEGASAFE for AdvFS  */
                                /* event logging.                          */
#define LOG_FTP         (11<<3) /* ftp daemon */
#define LOG_NTP         (12<<3) /* NTP subsystem */
#define LOG_SECURITY    (13<<3) /* security subsystems (firewalling, etc.) */
#define LOG_CONSOLE     (14<<3) /* /dev/console output */

/* other codes through 15 reserved for system use */
#define LOG_LOCAL0      (16<<3) /* reserved for local use */
#define LOG_LOCAL1      (17<<3) /* reserved for local use */
#define LOG_LOCAL2      (18<<3) /* reserved for local use */
#define LOG_LOCAL3      (19<<3) /* reserved for local use */
#define LOG_LOCAL4      (20<<3) /* reserved for local use */
#define LOG_LOCAL5      (21<<3) /* reserved for local use */
#define LOG_LOCAL6      (22<<3) /* reserved for local use */
#define LOG_LOCAL7      (23<<3) /* reserved for local use */

#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

/************************************************************************/
/*
  ACN specific defines
*/
#define LOG_RLP LOG_LOCAL0
#define LOG_SDT LOG_LOCAL1
#define LOG_NETI LOG_LOCAL2
#define LOG_SLP LOG_LOCAL3
#define LOG_DMP LOG_LOCAL4
#define LOG_MISC LOG_LOCAL5

#define ACNLOG_NONE 0
#define ACNLOG_SYSLOG 1
#define ACNLOG_STDOUT 2
#define ACNLOG_STDERR 3

#if CONFIG_ACNLOG == ACNLOG_SYSLOG

#include <syslog.h>

#define acnopenlog(ident, option, facility) openlog(ident, option, facility)
#define acncloselog() closelog()
#define acnlog(priority, ...) syslog(priority, __VA_ARGS__)
/* void  openlog (const char *ident, int option, int facility) */
/* void  syslog (int priority, const char *format, ...) */
/* void  closelog (void) */

#elif CONFIG_ACNLOG == ACNLOG_STDOUT

#include <stdio.h>

#define acnopenlog(ident, option, facility)
#define acncloselog()
#define acnlog(priority, ...) if (((priority) & 7) <= CONFIG_LOGLEVEL) printf(__VA_ARGS__)

#elif CONFIG_ACNLOG == ACNLOG_STDERR

#include <stdio.h>

#define acnopenlog(ident, option, facility)
#define acncloselog()
#define acnlog(priority, ...) if (((priority) & 7) <= CONFIG_LOGLEVEL) fprintf(stderr, __VA_ARGS__)

#else /* CONFIG_ACNLOG == ACNLOG_NONE */

#define acnopenlog(ident, option, facility)
#define acncloselog()
#define acnlog(priority, ...)

#endif

#endif
