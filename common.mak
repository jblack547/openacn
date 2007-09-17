##########################################################################
# 
# Copyright (c) 2007, Pathway Connectivity Inc.
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of Pathway Connectivity Inc. nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# 	$Id$
# 
##########################################################################

.PHONY: all
all::

_OUTTOP ?= .
_OUTTOP := $(_OUTTOP)/output
#default directory
OUT_DIR := common

.SUFFIXES:

# External programs
AR = arm-elf-ar
CC = arm-elf-gcc -g
LD = arm-elf-ld
OBJCOPY = arm-elf-objcopy

LDFLAGS  = $(CDEBUG) $(CARCH) -nostartfiles 

ifndef LDSCRIPT
  LDSCRIPT := at91/bb.ld
endif

CFLAGS := -MD -Wall -Werror

MODULES = at91 char_lcd crond ethsmsc flash malloc slave_cpu slp syslog tftp wat network cewi cdi acn \
acn_protocols acn_config pcserial ui 
include $(addsuffix /Module.mak,$(MODULES))
$(acn_BINARY) : $(malloc_BINARY) \
$(crond_BINARY) \
$(acn_config_BINARY) \
$(acn_protocols_BINARY) \
$(rlp_BINARY) \
$(sdt_BINARY) \
$(dmp_BINARY) \
$(pcserial_BINARY) \
$(char_lcd_BINARY) \
$(malloc_BINARY) \
$(slave_cpu_BINARY) \
$(slp_BINARY) \
$(cdi_BINARY) \
$(cewi_BINARY) \
$(syslog_BINARY) \
$(network_BINARY) \
$(tftp_BINARY) \
$(wat_BINARY) \
$(ethsmsc_BINARY) \
$(flash_BINARY) \
$(ui_BINARY) \
$(at91_BINARY)
 
.PHONY: install
install:: all
        #copy the compiled binaries
	cp $(subst .elf,.bin,$(acn_BINARY)) /tftpboot/$(FINAL_BINARY)
