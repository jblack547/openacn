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
# Include configuration stuff
# This is generated from the user configuration - see below
#
include .opts.mk

##########################################################################
# PLATFORM is normally in the platform directory
#
ifeq "${PLATFORM}" ""
PLATFORM=platform/${PLATFORMNAME}
endif

##########################################################################
#
# Find or create an object code directory and a library directory
#
ifeq "${PLATFORMNAME}" "win32"
MKDIR=mkdir
else
MKDIR=mkdir -p
endif

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

##########################################################################
# Default file separators and some C flags
# These should work on any gcc platform but of course Windows needs
# something different

# Output flag for CC
ifeq "${CCOUTPUT}" ""
CCOUTPUT:=-o
endif

# Output flag for CPP
ifeq "${CPPOUTPUT}" ""
CPPOUTPUT:=-o
endif

# Compile only (no link) flag for CC
ifeq "${CCONLY}" ""
CCONLY:=-c
endif

##########################################################################
# Set up some C compiler options and compile rules
#
ifeq "${COMPILER}" "gcc"
CFLAGS+=-std=c99 -pedantic -Wall -Wextra -Wno-uninitialized
CFLAGS+=-D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1
endif

##########################################################################
# Define include search for building - first build specifics, then general
# includes.
INCLUDEDIRS+=include
INCLUDEDIRS+=${ACNROOT}/include/arch-${ARCH}
INCLUDEDIRS+=${ACNROOT}/${PLATFORM}
INCLUDEDIRS+=${ACNROOT}/include

CPPFLAGS:=${CPPFLAGS} ${addprefix -I,${INCLUDEDIRS}}

##########################################################################
# Cancel all built in make rules axcept the ones we are likely to use
#
#.SUFFIXES: .o .ln .s .c .h .a .ln .cc .i
.SUFFIXES:

# Genric C build rule
${OBJDIR}/%.o: %.c
	${CC} ${CFLAGS} ${CPPFLAGS} ${TARGET_ARCH} ${CCONLY} ${CCOUTPUT}$@ $<

##########################################################################
# SUBDIRS defines all the sub-directories which are normally built
#
SUBDIRS:=${PLATFORM} common ${ACNPARTS}

SUBDIRPATHS:=${addprefix ${ACNROOT}/,${SUBDIRS}}
vpath %.c ${SUBDIRPATHS} ${ACNROOT}


##########################################################################
# For each subdir, we define a separate set of object files e.g. sdt_OBJS
# and also add them to the all_OBJS list
#
define OBJSFORDIR
${DIR}_SRC:=$${notdir $${wildcard ${addsuffix /*.c, ${ACNROOT}/${DIR}}}}
${DIR}_OBJS:=$${patsubst %.c,${OBJDIR}/%.o,$${${DIR}_SRC}}
all_OBJS+=$${${DIR}_OBJS}
endef

${foreach DIR, ${SUBDIRS}, ${eval ${OBJSFORDIR}}}
DEPS=${patsubst ${OBJDIR}/%.o, ${DEPDIR}/%.d, ${all_OBJS}}
##########################################################################
# Build rules
#
#Get the default rule in early

LIBRARY=${LIBDIR}/libacn.a

.PHONY : all

all : ${LIBRARY}

##########################################################################
# Define a target for each subdir, allowing "make sdt" and the like
#

.PHONY: ${SUBDIRS}

define SUBDIRRULE
${DIR}: ${${DIR}_OBJS}

endef

${foreach DIR, ${SUBDIRS}, ${eval ${SUBDIRRULE}}}

##########################################################################
# Put all objects into a library
#

${LIBRARY}: ${all_OBJS}
	ar rcs $@ $^

##########################################################################
# Housekeeping
#
.PHONY : clean

clean :
ifneq "${PLATFORMNAME}" "win32"
	rm -f *.defs .opts.mk ${OBJDIR}/*.o ${LIBRARY} ${DEPDIR}/*.d
else
	del /a .opts.mk opts.mk.i ${OBJDIR}\*.o ${subst /,\,${LIBRARY}} ${DEPDIR}\*.d
endif

##########################################################################
# Track dependencies
#
# Make sure we have its dependencies OK
ifeq "${COMPILER}" "gcc"

${DEPDIR}/%.d : %.c
	${CC} ${CFLAGS} ${CPPFLAGS} -MM -MG -MP $< \
	| sed 's/^\([^ :]\+\)\.o:/${OBJDIR}\/\1.o ${DEPDIR}\/\1.d:/' > $@

include ${DEPS}
endif

##########################################################################
# ts is a target for miscellaneous debug and doesn't do much
#
.PHONY : ts
ts:
	@echo ${COMPILER}

##########################################################################
# .opts.mk contains Make relevant options extracted from the
# configuration headers opt.h and user_opt.h using dummy C file opts.mk.c
#
ifneq "${PLATFORMNAME}" "win32"

.opts.mk: ${ACNROOT}/opts.mk.c ${ACNROOT}/include/opt.h include/user_opt.h
	${CPP} ${CPPFLAGS} $< | sed -e "/^[ \t]*$$/d" -e "/^[ \t]*#/d" > $@

else

%.i: ${ACNROOT}/%.c ${ACNROOT}/include/opt.h include/user_opt.h
	${CPP} ${CPPFLAGS} $< ${CPPOUTPUT}$@

.opts.mk: opts.mk.i
	sed -e "/^Borland/d" -e "/^[ \t]*#/d" -e "s@^/\*.*\*/@@" -e "/^ *$$/d" opts.mk.i > $@

endif


# ${DEPDIR}/opts.mk.d: opts.mk.c
# 	${CC} ${CFLAGS} ${CPPFLAGS} -MM -MG -MP $< \
# 	| sed 's/^\([^ :]\+\)\.o:/.\1 \1.d:/' > $@
# 
# include ${DEPDIR}/opts.mk.d

##########################################################################
# .defs produces a complete preprocessor dump of the symbols defined in a
# header, including predefined macros and any included headers. This is
# useful for debugging and also used by make for extracting config
# options.
#
vpath %.h ${INCLUDEDIRS}

%.defs: %.h
	${CPP} ${CPPFLAGS} -dM $< -o$@
