AM_CPPFLAGS =						\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DG_LOG_DOMAIN=\"Caja-Main-Menu\"		\
	-DDATADIR=\"$(datadir)\"			\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(CAJA_CFLAGS)				\
	$(WARN_CFLAGS)

caja_extensiondir=@CAJADIR@

caja_extension_LTLIBRARIES=libcaja-main-menu.la

libcaja_main_menu_la_SOURCES = \
	caja-main-menu.c       \
	caja-main-menu.h       \
	main-menu.c

libcaja_main_menu_la_LDFLAGS = -module -avoid-version
libcaja_main_menu_la_LIBADD  = $(CAJA_LIBS)
