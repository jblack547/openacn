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

XSLPARAMS:=
XSLPARAMS+=--stringparam draft.mode "${DRAFT}"
XSLPARAMS+=--stringparam release.level "${RELEASELEVEL}"
XSLPARAMS+=--stringparam vcs.version "${SVNVERSION}"

#
# Directories
#
SRCDIR:=src
HTMLDIR:=html

#
# Stylesheets
#
#XSLXHTML:=http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl
XSL_HTML:=${SRCDIR}/openacndoc.xsl


.PHONY : all

all : ${HTMLDIR}/index.html ${HTMLDIR}/openacn.html

${HTMLDIR}/index.html : ${SRCDIR}/*.docbook ${SRCDIR}/html.*.xsl ${SRCDIR}/common.*.xsl ${HTMLDIR}/openacndoc.css
	xsltproc --xinclude --output $@ ${SRCDIR}/html.chunk.xsl ${SRCDIR}/openacn.docbook

${HTMLDIR}/openacn.html : ${SRCDIR}/*.docbook  ${SRCDIR}/html.*.xsl ${SRCDIR}/common.*.xsl ${HTMLDIR}/openacndoc.css
	xsltproc --xinclude --output $@ ${SRCDIR}/html.monolithic.xsl ${SRCDIR}/openacn.docbook

${HTMLDIR}/%.html : ${SRCDIR}/%.docbook  ${SRCDIR}/html.*.xsl ${SRCDIR}/common.*.xsl
	xsltproc --xinclude --output $@ ${SRCDIR}/html.monolithic.xsl $<

${HTMLDIR}/%.css : ${SRCDIR}/%.css
	cp $< $@

clean :
	rm -f ${HTMLDIR}/*.html ${HTMLDIR}/*.css

error :
	echo Error: ${ERROR}
	false
