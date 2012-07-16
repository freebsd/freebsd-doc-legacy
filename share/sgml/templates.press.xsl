<?xml version="1.0" encoding="ISO-8859-1" ?>
<!DOCTYPE xsl:stylesheet PUBLIC "-//FreeBSD//DTD FreeBSD XSLT 1.0 DTD//EN"
				"http://www.FreeBSD.org/XML/www/share/sgml/xslt10-freebsd.dtd" [
<!ENTITY title "FreeBSD in the Press">
<!ENTITY email "freebsd-www">
<!ENTITY rsslink "press-rss.xml">
<!ENTITY rsstitle "&title;">
<!ENTITY % navinclude.about "INCLUDE">
<!ENTITY % header.rss "INCLUDE">
]>

<!-- $FreeBSD$ -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
  xmlns:cvs="http://www.FreeBSD.org/XML/CVS">

  <xsl:import href="http://www.FreeBSD.org/XML/www/lang/share/sgml/libcommon.xsl"/>
  <xsl:import href="http://www.FreeBSD.org/XML/www/share/sgml/xhtml.xsl"/>

  <xsl:param name="news.press.xml-master" select="'none'" />
  <xsl:param name="news.press.xml" select="'none'" />

  <xsl:template name="process.content">
              <div id="SIDEWRAP">
                &nav;
                <div id="FEEDLINKS">
                  <ul>
                    <li>
                      <a href="&rsslink;" title="FreeBSD in the Press RSS 2.0 Feed">
                        RSS 2.0 Feed
                      </a>
                    </li>
                  </ul>
                </div> <!-- FEEDLINKS -->
              </div> <!-- SIDEWRAP -->

	      <div id="CONTENTWRAP">

	      &header3;

		<xsl:for-each select="/press">
		<xsl:call-template name="html-news-list-press-preface" />

	<xsl:call-template name="html-news-list-press">
          <xsl:with-param name="news.press.xml-master" select="$news.press.xml-master" />
          <xsl:with-param name="news.press.xml" select="$news.press.xml" />
	</xsl:call-template>

		<xsl:call-template name="html-press-make-olditems-list" />

		<xsl:call-template name="html-news-list-newsflash-homelink" />
		</xsl:for-each>

	  	</div> <!-- CONTENTWRAP -->
		<br class="clearboth" />
  </xsl:template>
</xsl:stylesheet>
