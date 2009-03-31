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
#  * Neither the name of Engineering Arts nor the names of its
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
# to openacn\trunk or whatever tag you have downloaded).

ACNROOT=..\acnsrc

##########################################################################
# PLATFORMNAME is the name of your platform.
# This is often the processor  architecture combined with the stack.
# If you are building on a Linux/Unix system or under Cygwin, this and
# the compiler may be the only Make configuration items you need to
# change.
#
PLATFORMNAME:=win32

##########################################################################
# Systems using GCC can deduce ARCH, but Win cannot.
# This must match the ARCH_win32 option in opt.h/user_opt.h
ARCH:=i386

##########################################################################
# If you are corss compiling, you need to define yourC compiler here.
# If compiling is native, then it is usually best to leave this to Make.
#
VISUALCDIR:=C:\Program Files\Microsoft Visual Studio 9.0\VC
CC="${VISUALCDIR}\bin\cl.exe"

##########################################################################
# You may need to define the C preprocessor as well - not usually
# necessary for GCC because it can be accessed through CC, but other
# compilers are different.
#
CPP=${CC} /TC /E

##########################################################################
# CFLAGS are the compiler option flags
# Note include directories belong in CPPFLAGS not here
CFLAGS:=/O2
CFLAGS+=/Wall

##########################################################################
# Include any more build specific configuration or rules here
#

##########################################################################
# Now include rules common to all builds
#
# include ${ACNROOT}/builddir.mk


##########################################################################
# Include configuration stuff
# This is generated from the user configuration - see below
#
include .opts.mk

##########################################################################
# PLATFORM is normally in the platform directory
#
ifeq "${PLATFORM}" ""
PLATFORM=platform\${PLATFORMNAME}
endif

##########################################################################
#
# Find or create an object code directory and a library directory
#
MKDIR=mkdir

ifeq "${OBJDIR}" ""
OBJDIR:=obj
endif
ifneq "${wildcard ${OBJDIR}}" "${OBJDIR}"
${shell ${MKDIR} ${OBJDIR}}
endif

ifeq "${LIBDIR}" ""
LIBDIR:=lib
endif
ifneq "${wildcard ${LIBDIR}}" "${LIBDIR}"
${shell ${MKDIR} ${LIBDIR}}
endif

ifeq "${DEPDIR}" ""
DEPDIR:=deps
endif
ifneq "${wildcard ${DEPDIR}}" "${DEPDIR}"
${shell ${MKDIR} ${DEPDIR}}
endif

ifeq "${LOGDIR}" ""
LOGDIR:=log
endif
ifneq "${wildcard ${LOGDIR}}" "${LOGDIR}"
${shell ${MKDIR} ${LOGDIR}}
endif

##########################################################################
# Define include search for building - first build specifics, then general
# includes.
INCLUDEDIRS+=include
INCLUDEDIRS+=${ACNROOT}\include\arch-${ARCH}
INCLUDEDIRS+=${ACNROOT}\${PLATFORM}
INCLUDEDIRS+=${ACNROOT}\include

CPPFLAGS+=${addprefix -I,${INCLUDEDIRS}}

##########################################################################
# Cancel all built in make rules axcept the ones we are likely to use
#
#.SUFFIXES: .o .ln .s .c .h .a .ln .cc .i
.SUFFIXES:

DEPFILE=${DEPDIR}\${notdir ${basename $@}}.d
LOGFILE=${LOGDIR}\${notdir ${basename $@}}.log
# Genric C build rule
${OBJDIR}\\%.obj : %.c
	${CC} ${CFLAGS} ${CPPFLAGS} ${TARGET_ARCH} -showIncludes -c -Fo$@ $< > ${LOGFILE}
	sed -n -e "/[Pp]rogram [Ff]iles\\[Mm]icrosoft/d" -e "s,Note: including file:  *,${subst \,\\,$@}: ,p" ${LOGFILE} > ${DEPFILE}

##########################################################################
# SUBDIRS defines all the sub-directories which are normally built
#
SUBDIRS:=${PLATFORM} common ${ACNPARTS}

SUBDIRPATHS:=${addprefix ${ACNROOT}\,${SUBDIRS}}
C_VPATH:=${SUBDIRPATHS} ${ACNROOT}
vpath %.c ${C_VPATH}


##########################################################################
# For each subdir, we define a separate set of object files e.g. sdt_OBJS
# and also add them to the all_OBJS list
#
define OBJSFORDIR
${DIR}_SRC:=$${notdir $${wildcard ${ACNROOT}/${DIR}/*.c}}
${DIR}_OBJS:=$${patsubst %.c,${OBJDIR}\\%.obj,$${${DIR}_SRC}}
all_OBJS+=$${${DIR}_OBJS}
endef

${foreach DIR, ${SUBDIRS}, ${eval ${OBJSFORDIR}}}
##########################################################################
# Build rules
#
#Get the default rule in early

LIBRARY=${LIBDIR}\libacn.a

.PHONY : all

all : ${LIBRARY}

##########################################################################
# Define a target for each subdir, allowing "make sdt" and the like
#

.PHONY: ${SUBDIRS}

define SUBDIRRULE
${DIR}: $${${DIR}_OBJS}

endef

${foreach DIR, ${SUBDIRS}, ${eval ${SUBDIRRULE}}}

##########################################################################
# Put all objects into a library
#

${LIBRARY}: ${all_OBJS}
	@echo $^

##########################################################################
# Housekeeping
#
.PHONY : clean

clean :
ifneq "${PLATFORMNAME}" "win32"
	rm -f *.defs .opts.mk ${OBJDIR}\*.obj ${LIBRARY} ${DEPDIR}\*.d
else
	del /a .opts.mk opts.mk.i ${OBJDIR}\*.obj ${LIBRARY} ${DEPDIR}\*.d
endif

##########################################################################
# ts is a target for miscellaneous debug and doesn't do much
#
.PHONY : ts
ts:
	@echo ${C_VPATH} ${rlp_OBJS}

##########################################################################
# .opts.mk contains Make relevant options extracted from the
# configuration headers opt.h and user_opt.h using dummy C file opts.mk.c
#
ifneq "${PLATFORMNAME}" "win32"

.opts.mk: ${ACNROOT}\opts.mk.c ${ACNROOT}\include\opt.h include\user_opt.h
	${CPP} ${CPPFLAGS} $< | sed -e "/^[ \t]*$$/d" -e "/^[ \t]*#/d" > $@

else

%.i: ${ACNROOT}\%.c ${ACNROOT}\include\opt.h include\user_opt.h
	${CPP} ${CPPFLAGS} $< /P -Fo$@

.opts.mk: opts.mk.i
	sed -e "/^Borland/d" -e "/^[ \t]*#/d" -e "s@^/\*.*\*/@@" -e "/^ *$$/d" opts.mk.i > $@

endif

-include ${DEPDIR}/*.d
