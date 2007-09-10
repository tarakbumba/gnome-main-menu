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
schemas_DATA = application-browser.schemas gnome-main-menu.schemas
@INTLTOOL_SCHEMAS_RULE@

bookmarkdir = $(datadir)/$(PACKAGE)
bookmark_DATA = applications.xbel documents.xbel system-items.xbel places.xbel empty.ods

$(server_DATA).in: $(server_DATA).in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

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
	$(ab_desktop).in	\
	$(mm_desktop)		\
	$(schemas_DATA).in	\
	$(bookmark_DATA)	\
	$(ui_DATA)		\
	$(server_DATA).in.in	\
	$(glade_DATA)

CLEANFILES =			\
	$(ab_desktop)		\
	$(schemas_DATA)		\
	$(server_DATA).in	\
	$(server_DATA)
