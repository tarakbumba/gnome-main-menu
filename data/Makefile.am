serverdir   = $(libdir)/bonobo/servers
server_DATA = GNOME_MainMenu.server
@INTLTOOL_SERVER_RULE@

uidir   = $(datadir)/gnome-2.0/ui
ui_DATA = GNOME_MainMenu_ContextMenu.xml

gladedir = $(datadir)/$(PACKAGE)
glade_DATA = slab-window.glade menu-browser.glade

desktopdir   = $(datadir)/applications
ab_desktop   = application-browser.desktop
mm_desktop   = gnome-screensaver-lock.desktop gnome-session-kill.desktop
desktop_DATA = $(ab_desktop) $(mm_desktop)
@INTLTOOL_DESKTOP_RULE@

schemasdir   = @GCONF_SCHEMA_FILE_DIR@
ab_schemas   = application-browser.schemas
mm_schemas   = slab.schemas
schemas_DATA = $(ab_schemas) $(mm_schemas)
@INTLTOOL_SCHEMAS_RULE@

bookmarkdir = $(datadir)/$(PACKAGE)
bookmark_DATA = applications.xbel documents.xbel system-items.xbel places.xbel empty.ods

$(ab_desktop).in: $(ab_desktop).in.in
	sed -e "s|\@BINDIR\@|$(bindir)|" $< > $@

$(server_DATA).in: $(server_DATA).in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

$(mm_schemas).in: $(mm_schemas).in.in
	sed -e "s|\@DATADIR\@|$(datadir)|g" $< > $@

install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi

if ENABLE_DYNAMIC_LIBSLAB
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libslab.pc
endif

EXTRA_DIST =			\
	$(ab_desktop).in.in	\
	$(mm_desktop)		\
	$(ab_schemas).in	\
	$(mm_schemas).in.in	\
	$(bookmark_DATA)	\
	$(ui_DATA)		\
	$(server_DATA).in.in	\
	$(glade_DATA)

CLEANFILES =			\
	$(ab_desktop).in	\
	$(desktop_DATA)		\
	$(mm_schemas).in	\
	$(schemas_DATA)		\
	$(server_DATA).in
