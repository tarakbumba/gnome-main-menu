INCLUDES =								\
	$(MAIN_MENU_CFLAGS)						\
	-I$(top_srcdir)/libslab						\
	$(WARN_CFLAGS)

libexec_PROGRAMS = main-menu

AM_CPPFLAGS =								\
	$(GLADE_CFLAGS)							\
	-DDATADIR=\""$(datadir)"\"					\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

main_menu_SOURCES =							\
	directory-tile.c		directory-tile.h		\
	directory-tile-model.c		directory-tile-model.h		\
	document-tile.c			document-tile.h			\
	eggaccelerators.c		eggaccelerators.h		\
	file-tile-model.c		file-tile-model.h		\
	main-menu.c							\
	main-menu-migration.c		main-menu-migration.h		\
	main-menu-ui.c			main-menu-ui.h			\
	network-tile.c			network-tile.h			\
	network-tile-model.c		network-tile-model.h		\
	storage-tile.c			storage-tile.h			\
	storage-tile-model.c		storage-tile-model.h		\
	system-tile.c			system-tile.h			\
	tile-table.c			tile-table.h			\
	tomboykeybinder.c		tomboykeybinder.h

main_menu_LDADD =							\
	$(MAIN_MENU_LIBS)						\
	$(top_srcdir)/libslab/.libs/libslab.a
