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
# Common make rules for ACN build directories
#
# This Makefile provides most of the actual rules which are common to all 
# build configurations.
#
# Although in the top directory, it is read in the context of a build
# directory.
#
# Configuration which varies between builds should go in the Makefile in
# the build directory not here.
#
##########################################################################
# If ARCH isn't already defined, try to deduce it
#
ifeq "${ARCH}" ""
include .arch.mk
endif

##########################################################################
# Define include search for building - first build specifics, then general
# includes.
INCLUDEDIRS+=${BUILDDIR}/include ${TOPDIR}/include ${TOPDIR}/include/arch-${ARCH} ${PLATFORM}

CPPFLAGS:=${addprefix -I,${INCLUDEDIRS}}
CONFIG:=${TOPDIR}/include/opt.h

export CPPFLAGS CONFIG

##########################################################################
# Cancel all built in make rules axcept the ones we are likely to use
#
.SUFFIXES: .o .ln .s .c .h .a .ln .cc .i

##########################################################################
#
# Find or create an object code directory and a library directory
#
ifeq "${OBJDIR}" ""
OBJDIR:=${BUILDDIR}/obj
endif
ifneq "${wildcard ${OBJDIR}}" "${OBJDIR}"
${shell mkdir -p ${OBJDIR}}
endif

ifeq "${LIBDIR}" ""
LIBDIR:=${BUILDDIR}/lib
endif
ifneq "${wildcard ${LIBDIR}}" "${LIBDIR}"
${shell mkdir -p ${LIBDIR}}
endif

export OBJDIR LIBDIR

##########################################################################
# SUBDIRS defines all the sub-directories which are normally built
#
SUBDIRS:=common rlp sdt slp dmp ${PLATFORM} 

##########################################################################
# Build rules
#

.PHONY : all subdirs ${SUBDIRS}

all : subdirs

subdirs: ${SUBDIRS}

${SUBDIRS}:
	${MAKE} -C ${TOPDIR}/$@

.PHONY : clean

clean : ${addsuffix _clean, ${SUBDIRS}}
	rm -f *.defs .arch.mk

${addsuffix _clean, ${SUBDIRS}}:
	${MAKE} -C ${TOPDIR}/${@:_clean=} clean

##########################################################################
# ts is a target for miscellaneous debug and doesn't do much
.PHONY : ts
ts:
	@echo ${.DEFAULT_GOAL}

##########################################################################
# .defs produces a complete preprocessor dump of the symbols defined in a
# header, including predefined macros and any included headers. This is
# useful for debugging and also used by make for extracting config
# options.
#
vpath %.h ${INCLUDEDIRS}

%.defs: %.h
	${CPP} ${CPPFLAGS} -dM $< -o$@

##########################################################################
# Explicit additional dependency for opt.h since it changes a lot
opt.defs: user_opt.h

##########################################################################
# .arch.mk is used by Make to deduce the architecture
.arch.mk: opt.defs
	sed -n 's/#define ARCH_\([^ ]\+\) 1$$/ARCH:=\1/p' $< > $@
