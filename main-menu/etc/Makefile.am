serverdir   = $(libdir)/bonobo/servers
server_DATA = GNOME_MainMenu.server
@INTLTOOL_SERVER_RULE@

@INTLTOOL_XML_NOMERGE_RULE@
gsettings_SCHEMAS = org.mate.gnome-main-menu.gschema.xml
@GSETTINGS_RULES@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@DATADIR\@^$(datadir)^g' -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_MainMenu_ContextMenu.xml

desktop_helperdir = $(datadir)/applications
desktop_helper_DATA = gnome-screensaver-lock.desktop gnome-session-logout.desktop gnome-session-shutdown.desktop

desktopdir   = $(datadir)/applications
desktop_DATA = trigger-panel-run-dialog.desktop
@INTLTOOL_DESKTOP_RULE@

bookmarkdir = $(datadir)/gnome-main-menu
bookmark_DATA = applications.xbel documents.xbel places.xbel empty.ots empty.ott

systemitemsdir = $(datadir)/gnome-main-menu
systemitems_DATA = system-items.xbel

$(desktop_DATA).in: $(desktop_DATA).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

$(server_DATA).in: $(server_DATA).in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

$(systemitems_DATA): $(systemitems_DATA).in
	sed -e "s|_title>|title>|g" $< > $@

EXTRA_DIST = $(server_DATA).in.in $(gsettings_SCHEMAS).in.in $(ui_DATA) $(desktop_helper_DATA) $(desktop_DATA).in.in $(bookmark_DATA) $(systemitems_DATA).in

CLEANFILES = $(server_DATA) $(gsettings_SCHEMAS) $(server_DATA).in $(gsettings_SCHEMAS).in $(systemitems_DATA) $(desktop_DATA) $(desktop_DATA).in
