AC_INIT([gnome-main-menu], [1.8.0], [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-main-menu])

AC_CONFIG_SRCDIR([.])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar check-news])
AM_MAINTAINER_MODE

AC_SUBST([ACLOCAL_AMFLAGS], ["\${ACLOCAL_FLAGS}"])

GNOME_COMPILE_WARNINGS

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AM_PROG_LIBTOOL

dnl ==============================================
dnl translation support
dnl ==============================================

GETTEXT_PACKAGE=gnome-main-menu
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])

IT_PROG_INTLTOOL([0.35.0])
AM_GLIB_GNU_GETTEXT

dnl ==============================================
dnl Check that we meet the dependencies
dnl ==============================================

GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.18
SLAB_REQUIRED=1.5.2

AM_PATH_GLIB_2_0($GLIB_REQUIRED,,,gobject)

COMMON_MODULES="glib-2.0 >= $GLIB_REQUIRED	\
		gio-2.0 >= $GLIB_REQUIRED 	\
		gio-unix-2.0			\
		gobject-2.0 			\
		gtk+-2.0 >= $GTK_REQUIRED 	\
		gdk-2.0				\
		libslab >= $SLAB_REQUIRED"


PKG_CHECK_MODULES(APPLICATION_BROWSER, [ $COMMON_MODULES mate-desktop-2.0 unique-1.0 ])
AC_SUBST(APPLICATION_BROWSER_CFLAGS)
AC_SUBST(APPLICATION_BROWSER_LIBS)

PKG_CHECK_MODULES(MAIN_MENU, [ $COMMON_MODULES libmatepanelapplet-4.0 mate-desktop-2.0 gio-2.0 libgtop-2.0 libxml-2.0 x11 cairo ])
AC_SUBST(MAIN_MENU_CFLAGS)
AC_SUBST(MAIN_MENU_LIBS)

# Check for network support
NM_GLIB=
PKG_CHECK_EXISTS(libnm-glib, [ NM_GLIB=libnm-glib ],
		 PKG_CHECK_EXISTS(libnm_glib, [ NM_GLIB=libnm_glib ]))
if test "x$NM_GLIB" = "x"; then
	# make sure we have something set for the call to PKG_CHECK_MODULES
	NM_GLIB=libnm-glib
fi

PKG_CHECK_MODULES(NETWORKMANAGER, [NetworkManager $NM_GLIB >= 0.8.0 libnm-util >= 0.8.0], HAVE_NM=1, HAVE_NM=0)
if test "x$HAVE_NM" != "x1"; then
	AC_WARN(NetworkManager is not available)
fi

AC_CHECK_HEADERS(iwlib.h, [ AC_CHECK_LIB(iw, iw_sockets_open, HAVE_IWLIB=1, HAVE_IWLIB=0, -lm)], HAVE_IWLIB=0)
if test "x$HAVE_IWLIB" != "x1"; then
	AC_WARN(iwlib is not available)
fi

HAVE_NETWORK=0
NETWORK_CFLAGS=
NETWORK_LIBS=
if test "x$HAVE_NM" != "x1" -o "x$HAVE_IWLIB" != "x1"; then
	AC_WARN(network support disabled)
else
	HAVE_NETWORK=1
	NETWORK_CFLAGS="$NETWORKMANAGER_CFLAGS"
	NETWORK_LIBS="$NETWORKMANAGER_LIBS -liw -lm"
	AC_DEFINE(HAVE_NETWORK, 1, [Use NetworkManager and wireless library])
fi

AM_CONDITIONAL(HAVE_NETWORK, test "x$HAVE_NETWORK" = "x1")
AC_SUBST(NETWORK_CFLAGS)
AC_SUBST(NETWORK_LIBS)

dnl ==============================================
dnl GSettings
dnl ==============================================

GLIB_GSETTINGS

dnl ==============================================
dnl End: GSettings
dnl ==============================================

dnl ==============================================
dnl Check strftime for %l and %k support
dnl ==============================================

AC_MSG_CHECKING(for %l and %k support in strftime)
AC_TRY_RUN([
#include <string.h>
#include <time.h>

int main(int argc, char **argv)
{
	char buf[10];
	time_t rawtime;
	struct tm *timeinfo;

	time(&rawtime);
	timeinfo=localtime(&rawtime);
	buf[0] = '\0';
	strftime(buf, 10, "%lx%k", timeinfo);

	if (buf[0] == '\0' || buf[0] == 'x' || strstr(buf, "l") || strstr(buf, "k"))
		exit(1);
	else
		exit(0);
}],[
AC_DEFINE(HAVE_LKSTRFTIME, 1, [strftime supports use of l and k])
ac_cv_lkstrftime=yes
],ac_cv_lkstrftime=no,ac_cv_lkstrftime=no,[
AC_DEFINE(HAVE_LKSTRFTIME, 1, [strftime supports use of l and k])
ac_cv_lkstrftime=yes
])
AC_MSG_RESULT($ac_cv_lkstrftime)

dnl ==============================================
dnl End: strftime
dnl ==============================================

dnl ==============================================
dnl Caja Extension
dnl ==============================================

ENABLE_PROFILER=
AC_ARG_ENABLE(caja-extension,
 AC_HELP_STRING([--enable-caja-extension], [Enable caja extension]),
 [ENABLE_CAJA_EXTENSION=1])

if test "x$ENABLE_CAJA_EXTENSION" = "x1"
then
	CAJA_REQUIRED=1.5.0

	PKG_CHECK_MODULES(CAJA, [libcaja-extension >= $NAUTILUS_REQUIRED glib-2.0 gobject-2.0 gio-2.0])
	CAJADIR=`pkg-config --variable=extensiondir libcaja-extension`

	AC_SUBST(CAJA_CFLAGS)
	AC_SUBST(CAJA_LIBS)
	AC_SUBST(CAJADIR)
fi

AM_CONDITIONAL(ENABLE_CAJA_EXTENSION, test "x$ENABLE_CAJA_EXTENSION" = "x1")


dnl ==============================================
dnl End: Caja Extension
dnl ==============================================

GTK_DOC_CHECK(1.0)

AC_CONFIG_FILES([
Makefile
application-browser/etc/Makefile
application-browser/src/Makefile
main-menu/etc/Makefile
main-menu/src/Makefile
caja-main-menu/Makefile
patch/Makefile
po/Makefile.in
])

AC_OUTPUT
