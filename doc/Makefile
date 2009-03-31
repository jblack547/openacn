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
# $Id$
#
#tabs=8
##########################################################################
#
# Some default variables - override on command line if desired
#
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

ifeq "${FODIR}" ""
FODIR:=./fo
endif

ifeq "${PDFDIR}" ""
PDFDIR:=./pdf
endif

ifeq "${TXTDIR}" ""
TXTDIR:=./txt
endif

ifeq "${PSDIR}" ""
PSDIR:=./ps
endif

ifeq "${RTFDIR}" ""
RTFDIR:=./rtf
endif

ifeq "${WMLDIR}" ""
WMLDIR:=./wml
endif

ifeq "${DEPDIR}" ""
DEPDIR:=./.deps
endif

##########################################################################
# Source files
#
# Assume all docbook files are sources
SOURCES=${wildcard ${DOCDIR}/*.docbook}

##########################################################################
# Sort out some fop commands
FOPCMD = fop
#FOPCMD += -c ${CONF}/fop0.9x.conf.xml
#
# Set general FOPCMD options
#
XSLPARAMS:=--stringparam fop1.extensions 1

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
# Main targets
#
.PHONY : all chunks singlepage

all : singlepage chunks

chunks: ${HTMLDIR}/openacndoc.css ${HTMLDIR}/index.html

singlepage: ${HTMLDIR}/openacndoc.css ${HTMLDIR}/openacn.html

pdf: ${PDFDIR}/openacn.pdf

${DOCDIR}/openacn.docbook: ${filter-out ${DOCDIR}/openacn.docbook,${SOURCES}}
	touch $@

##########################################################################
# XSLT rules
#

#
# Chunked HTML output
#
${HTMLDIR}/index.html : ${DOCDIR}/openacn.docbook ${XSLDIR}/html.*.xsl ${XSLDIR}/common.*.xsl
	@echo XSL generating $@
	@xsltproc --xinclude ${XSLPARAMS} --output $@ ${XSLDIR}/html.chunk.xsl $<
	@mkdir -p ${DEPDIR}
	@echo -n $@: > ${DEPFILE}
	@xsltproc --stringparam rootfile $< ${XSLDIR}/xdep.xsl $< >> ${DEPFILE}

#
# Single-page HTML output will work with any chapter
#
${HTMLDIR}/%.html : ${DOCDIR}/%.docbook ${XSLDIR}/html.*.xsl ${XSLDIR}/common.*.xsl
	@echo XSL generating $@
	@xsltproc --xinclude ${XSLPARAMS} --output $@ ${XSLDIR}/html.monolithic.xsl $<
	@mkdir -p ${DEPDIR}
	@echo -n $@: > ${DEPFILE}
	@xsltproc --stringparam rootfile $< ${XSLDIR}/xdep.xsl $< >> ${DEPFILE}

##########################################################################
# FO output
#
.PRECIOUS : ${FODIR}/%.fo

${FODIR}/%.fo : ${DOCDIR}/%.docbook ${XSLDIR}/fo.*.xsl ${XSLDIR}/common.*.xsl
	xsltproc --xinclude ${XSLPARAMS} --output $@ ${XSLDIR}/fo.book.xsl $<

# 
# Make various formats from FO source
#
${PDFDIR}/%.pdf : ${FODIR}/%.fo
ifneq "${wildcard ${PDFDIR}}" "${PDFDIR}"
	mkdir -p ${PDFDIR}
endif
	${FOPCMD} -fo $^ -pdf $@

${TXTDIR}/%.txt : ${FODIR}/%.fo
	${FOPCMD} -fo $^ -txt $@

${PSDIR}/%.ps : ${FODIR}/%.fo
	${FOPCMD} -fo $^ -ps $@

${RTFDIR}/%.rtf : ${FODIR}/%.fo
	fo2rtf $< $@

${WMLDIR}/%.wml : ${FODIR}/%.fo
	fo2wml $< $@


##########################################################################
# Put the CSS stylesheet in place
#
${HTMLDIR}/%.css : ${CSSDIR}/%.css
	@echo Copying $@
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
