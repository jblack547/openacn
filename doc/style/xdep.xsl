<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
    xmlns:xi="http://www.w3.org/2001/XInclude"
>
<xsl:output method="text"/>

<xsl:param name="rootfile" select="."/>

<xsl:template match="/">
<!--
<xsl:value-of select="$rootfile"/>:<xsl:text/>
-->
<xsl:apply-templates select="//xi:include">
	<xsl:with-param name="root">
		<xsl:call-template name="path-part">
			<xsl:with-param name="str" select="$rootfile"/>
		</xsl:call-template>
	</xsl:with-param>
</xsl:apply-templates>
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="xi:include">
<xsl:param name="base" select="/"/>
<xsl:param name="root" select="''"/>
<xsl:text> </xsl:text>
<xsl:value-of select="$root"/><xsl:value-of select="@href"/>
<xsl:variable name="doc" select="document(@href,$base)"/>
<xsl:apply-templates select="$doc//xi:include">
	<xsl:with-param name="base" select="$doc"/>
	<xsl:with-param name="root">
		<xsl:value-of select="$root"/>
		<xsl:call-template name="path-part">
			<xsl:with-param name="str" select="@href"/>
		</xsl:call-template>
	</xsl:with-param>
</xsl:apply-templates>
</xsl:template>

<xsl:template name="path-part">
  <xsl:param name="str"/>
	<xsl:if test="contains($str,'/')">
		<xsl:value-of select="concat(substring-before($str,'/'),'/')"/>
		<xsl:call-template name="path-part">
			<xsl:with-param name="str" select="substring-after($str,'/')"/>
		</xsl:call-template>
	</xsl:if>
</xsl:template>

</xsl:stylesheet>
