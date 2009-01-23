##########################################################################
#
# Makefile for openACN documentation
#
# Copyright (c) Philip Nye, Engineering Arts. All rights reserved.
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
#
# $Id: Makefile 327 2008-03-26 11:32:49Z philip $
#
#tabs=8
##########################################################################
#
# Some default variables - override on command line if desired
#
FOP:=fop9x
SVNVERSION:=${shell svnversion}

##########################################################################
# Directories
# Predefine SRCROOT HTMLDIR and DEPDIR and simply include this Makefile
# if you want to build in a different directory
#
ifeq "${SRCROOT}" ""
SRCROOT:=.
endif

ifeq "${HTMLDIR}" ""
HTMLDIR:=./html
endif

ifeq "${DEPDIR}" ""
DEPDIR:=./dep
endif

##########################################################################
# Main targets
#
.PHONY : all chunks singlepage

all : singlepage chunks

chunks: ${HTMLDIR}/openacndoc.css ${HTMLDIR}/index.html

singlepage: ${HTMLDIR}/openacndoc.css ${HTMLDIR}/openacn.html

##########################################################################
# Subdirectories
#
XSLDIR:=${SRCROOT}/style
CSSDIR:=${SRCROOT}/style
DOCDIR:=${SRCROOT}/docbook

##########################################################################
# Some parameters
#
XSLPARAMS:=
XSLPARAMS+=--stringparam draft.mode "${DRAFT}"
XSLPARAMS+=--stringparam release.level "${RELEASELEVEL}"
XSLPARAMS+=--stringparam vcs.version "${SVNVERSION}"

##########################################################################
# A macro defining a dependency file name
#
DEPFILE=${DEPDIR}/${basename ${notdir $@}}.d

##########################################################################
# XSLT rules
#

#
# Chunked output
#
${HTMLDIR}/index.html : ${DOCDIR}/openacn.docbook ${XSLDIR}/html.*.xsl ${XSLDIR}/common.*.xsl
	@echo XSL generating $@
	@xsltproc --xinclude ${XSLPARAMS} --output $@ ${XSLDIR}/html.chunk.xsl $<
	@echo -n $@: > ${DEPFILE}
	@xsltproc --stringparam rootfile $< ${XSLDIR}/xdep.xsl $< >> ${DEPFILE}

#
# Single-page output will work with any chapter
#
${HTMLDIR}/%.html : ${DOCDIR}/%.docbook  ${XSLDIR}/html.*.xsl ${XSLDIR}/common.*.xsl
	@echo XSL generating $@
	@xsltproc --xinclude ${XSLPARAMS} --output $@ ${XSLDIR}/html.monolithic.xsl $<
	@echo -n $@: > ${DEPFILE}
	@xsltproc --stringparam rootfile $< ${XSLDIR}/xdep.xsl $< >> ${DEPFILE}

##########################################################################
# Put the CSS stylesheet in place
#
${HTMLDIR}/%.css : ${CSSDIR}/%.css
	@Copying $@
	@cp $< $@

##########################################################################
# ts is used as a target in debugging
#
.PHONY: ts
ts :
	@echo ${patsubst ${DOCDIR}/%.docbook,${DEPDIR}/%.d,${DOCDIR}/openacn.docbook}

##########################################################################
# Cleanup
#
clean :
	rm -f ${HTMLDIR}/*.html ${HTMLDIR}/*.css

##########################################################################
# Include any dependencies
#
-include ${DEPDIR}/*.d
