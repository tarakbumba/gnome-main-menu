if ENABLE_NAUTILUS_EXTENSION
NAUTILUS_DIR=nautilus-main-menu
else
NAUTILUS_DIR=
endif

INCLUDES =						\
	$(MAIN_MENU_CFLAGS)				\
	-I$(top_srcdir)/libslab				\
	$(WARN_CFLAGS)

bin_PROGRAMS = tile-test

tile_test_SOURCES =					\
	tile-test.c

tile_test_LDADD =					\
	$(MAIN_MENU_LIBS)				\
	$(top_builddir)/libslab/libslab.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

SUBDIRS =				\
	libslab				\
	main-menu/etc			\
	main-menu/src			\
	$(NAUTILUS_DIR)			\
	patch				\
	po

EXTRA_DIST =				\
	intltool-extract.in		\
	intltool-merge.in		\
	intltool-update.in

DISTCLEANFILES =			\
	intltool-extract		\
	intltool-merge			\
	intltool-update
