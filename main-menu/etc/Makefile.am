appletdir       = $(datadir)/mate-panel/applets
applet_in_files = org.mate.GNOMEMainMenu.mate-panel-applet.in
applet_DATA     = $(applet_in_files:.mate-panel-applet.in=.mate-panel-applet)

$(applet_in_files): $(applet_in_files).in Makefile
	$(AM_V_GEN) $(SED) \
		-e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
		$< > $@

%.mate-panel-applet: %.mate-panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.mate.panel.applet.GNOMEMainMenuFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.mate.panel.applet.GNOMEMainMenuFactory.service: $(service_in_files)
	$(AM_V_GEN) $(SED) \
		-e "s|\@LIBEXECDIR\@|$(libexecdir)|" \
		$< > $@

@INTLTOOL_XML_NOMERGE_RULE@
gsettings_SCHEMAS = org.mate.gnome-main-menu.gschema.xml
@GSETTINGS_RULES@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@DATADIR\@^$(datadir)^g' -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

uidir   = $(datadir)/$(PACKAGE)
ui_DATA = GNOME_MainMenu_ContextMenu.xml

desktop_helperdir = $(datadir)/applications
desktop_helper_DATA = mate-screensaver-lock.desktop mate-session-logout.desktop mate-session-shutdown.desktop

desktopdir   = $(datadir)/applications
desktop_DATA = trigger-panel-run-dialog.desktop
@INTLTOOL_DESKTOP_RULE@

bookmarkdir = $(datadir)/mate-control-center
bookmark_DATA = applications.xbel documents.xbel places.xbel empty.ots empty.ott

systemitemsdir = $(datadir)/mate-control-center
systemitems_DATA = system-items.xbel

$(desktop_DATA).in: $(desktop_DATA).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

$(systemitems_DATA): $(systemitems_DATA).in
	sed -e "s|_title>|title>|g" $< > $@

EXTRA_DIST = $(applet_in_files).in $(service_in_files) $(gsettings_SCHEMAS).in.in $(ui_DATA) $(desktop_helper_DATA) $(desktop_DATA).in.in $(bookmark_DATA) $(systemitems_DATA).in

CLEANFILES = $(applet_DATA) $(applet_in_files) org.mate.panel.applet.GNOMEMainMenuFactory.service $(gsettings_SCHEMAS) $(gsettings_SCHEMAS).in $(systemitems_DATA) $(desktop_DATA) $(desktop_DATA).in
