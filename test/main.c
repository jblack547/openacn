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
/**
 * The main proggy.
 * Detects input or output from arch_hw_type
 * and runs only that code.
 */
const char *rcsid __attribute__ ((unused)) =
   "Id:$Header: /home/cvs/onyx/acn/main.c,v 1.10 2007/03/06 23:18:46 kloewen Exp $";

#define ACN_FIRMWARE

// libc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// TCP/IP stack
#include <wattcp.h>

// bsp
#include <arch.h>
#include <crond.h>
//#include <eth.h>
#include <flash.h>
#include <memorymap.h>
#include "image.h"
#include <syslog.h>
#include <tftps.h>
#include <button.h>
#include <ppmalloc.h>
#include <char_lcd.h>

// app
#include <acn_protocols.h>
#include <acn_config.h>
#include <slp.h>
//#include <magic_bullet.h>
#include <slave_cpu.h>
#include <network.h>
#include <cdi.h>
#include <cewi.h>
#include <ui.h>


static void rebooting(const char *msg)
{
   syslog(LOG_USER | LOG_INFO, msg);
	lcdTopText(msg, strlen(msg),0);
	sleep(1);
}

static void tcpTick()
{
   tcp_tick(0);
   cewi_tick();
   cdi_tick();
}



static void acn(void)
{
   addTask(1, tcpTick, -1);

   while(1)
   {
      crondTick();
   }
}

static void put_upgrade_firmware(char *filename, void *buffer, uint32 buffer_length)
{
	image_info *image = buffer;

	if(buffer_length) //transfer successful
	{
		if((image->jmp == IMAGE_JMP) &&
			(checksum(image, buffer_length) == CHECKSUM_GOOD))
		{
			flash_memcpy((void*)FLASH_IMAGE, buffer, buffer_length);
			die("Push Firmware Upgrade");
		}
	}
	syslog(LOG_USER | LOG_INFO, "Firmware Upgrade Failed");
}


void init(void)
{
	image_info *image;
	
   arch_init();
	
   srand(arch_serial);

   die_log = rebooting;

   rtclock_init();

   initCharLcd();

	lcdTopText("Task Scheduler", 14, 0);
	lcdBottomText("Starting", 8, 0);
   crondInit();
   
	lcdTopText("Dynamic Memory", 14, 0);	
   ppMallocInit();
	
	
   initPioScanner();
	lcdTopText("Peripherals", 11, 0);
	initSlaveCpu();
	
	selfTest();
	
   reset_etherchip();


   loadConfig();

	lcdTopText("Network Stack", 13, 0);
	lcdBottomText("Starting", 8, 0);
   sock_init();
	
   networkInit();
  
   syslog_init(0xefffed01);
   
   initSlp();
	
	activateConfig();

   setbuf(stdout, NULL);
	lcdTopText("ACN Stack", 9, 0);
	initAcnProtocols();
	lcdTopText("TFTP Server", 11, 0);	
   initTftpServer();
	
	image = (void *)FLASH_ALT_IMAGE;
	registerTftpFile(firmwareFile, (void *)image, image->length, FLASH_IMAGE_SIZE, put_upgrade_firmware);
	registerTftpFile("loader.bin", (void *)FLASH_BOOTLOADER, FLASH_BOOTLOADER_SIZE, FLASH_BOOTLOADER_SIZE, 0);
	registerTftpFile("sno.bin", (void *)FLASH_SERIAL, 4, 4, 0);
	ui_init();
}


int main()
{
   init();
   acn();
   //this will not return
   return 0;
}
