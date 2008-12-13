##########################################################################
# 
# Copyright (c) 2007, Engineering Arts (UK)
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
#
# Makefile for ACN build directory
#
# This Makefile provides major configuration options. Most of the actual 
# rules are common to all build configurations and are in 
# ${ACNROOT}/build.make which is included here
#
##########################################################################
# ACNROOT is the route to the OpenACN top level directory (corresponding
# to openacn/trunk or whatever tag you have downloaded).

ACNROOT=..

##########################################################################
# PLATFORMNAME is the name of your platform.
# This is often the processor  architecture combined with the stack.
#
PLATFORMNAME:=lwip

##########################################################################
# If you are corss compiling, you need to define yourC compiler here.
# If compiling is native, then it is usually best to leave this to Make.
#
COMPILER:=gcc
CC=/usr/x86_64-pc-linux-gnu/h8300-elf/gcc-bin/4.3.2/h8300-elf-gcc -ms

##########################################################################
# You may need to define the C preprocessor as well - not usually
# necessary for GCC because it can be accessed through CC, but other
# compilers are different.
#
# CPP:=${CC} -E

##########################################################################
# Need to access some LWIP stuff
#
LWIPDIR=${ACNROOT}/../lwip
INCLUDEDIRS+=${LWIPDIR}/src/include

##########################################################################
# Include any more build specific configuration or rules here
#

##########################################################################
# Now include rules common to all builds
#
include ${ACNROOT}/builddir.mk
