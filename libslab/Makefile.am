INCLUDES =					\
	$(LIBSLAB_CFLAGS)			\
	$(WARN_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"

lib_LTLIBRARIES = libslab.la

libslab_la_SOURCES =				\
	application-tile.c			\
	bookmark-agent.c			\
	context-menu-view.c			\
	desktop-item-tile-model.c		\
	double-click-detector.c			\
	egg-recent-item.c			\
	egg-recent-item.h			\
	egg-recent-model.c			\
	egg-recent-model.h			\
	grid-view.c				\
	libslab-utils.c				\
	menu-browser.c				\
	tile-attribute.c			\
	tile-button-view.c			\
	tile.c					\
	tile-control.c				\
	tile-model.c				\
	tile-view.c

libslab_includedir = $(includedir)/slab
libslab_include_HEADERS =			\
	application-tile.h			\
	bookmark-agent.h			\
	bookmark-agent-libslab.h		\
	context-menu-view.h			\
	desktop-item-tile-model.h		\
	double-click-detector.h			\
	grid-view.h				\
	libslab-utils.h				\
	menu-browser.h				\
	tile-attribute.h			\
	tile-button-view.h			\
	tile-control.h				\
	tile.h					\
	tile-model.h				\
	tile-view.h

libslab_la_LIBADD = $(LIBSLAB_LIBS)
