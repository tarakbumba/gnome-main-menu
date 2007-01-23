INCLUDES =								\
	$(MAIN_MENU_CFLAGS)						\
	-I$(top_srcdir)/libslab						\
	$(WARN_CFLAGS)

libexec_PROGRAMS = main-menu

AM_CPPFLAGS =								\
	$(GLADE_CFLAGS)							\
	-DDATADIR=\""$(datadir)"\"					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

main_menu_SOURCES =					\
	main-menu.c					\
	main-menu-ui.c		main-menu-ui.h		\
	main-menu-migration.c	main-menu-migration.h	\
	tile-table.c		tile-table.h		\
	apps-agent.c		apps-agent.h

main_menu_LDADD =							\
	$(MAIN_MENU_LIBS)						\
	$(GLADE_LIBS)							\
	$(top_builddir)/libslab/libslab.la
