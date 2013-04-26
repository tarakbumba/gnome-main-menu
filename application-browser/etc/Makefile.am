desktopdir       = $(datadir)/applications
desktop_DATA     = application-browser.desktop
@INTLTOOL_DESKTOP_RULE@

@INTLTOOL_XML_NOMERGE_RULE@
gsettings_SCHEMAS = org.mate.gnome-main-menu.application-browser.gschema.xml
@GSETTINGS_RULES@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

$(desktop_DATA).in: $(desktop_DATA).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

man_MANS = application-browser.1

EXTRA_DIST = $(desktop_DATA).in.in $(gsettings_SCHEMAS).in.in $(man_MANS)

CLEANFILES = $(desktop_DATA) $(gsettings_SCHEMAS) $(desktop_DATA).in $(gsettings_SCHEMAS).in
