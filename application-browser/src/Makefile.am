AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(APPLICATION_BROWSER_CFLAGS)				\
	$(WARN_CFLAGS)

bin_PROGRAMS = application-browser

application_browser_SOURCES =					\
	application-browser.c

application_browser_LDADD =					\
	$(APPLICATION_BROWSER_LIBS)
