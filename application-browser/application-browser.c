/*
 * This file is part of the Application Browser.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * The Application Browser is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Application Browser is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Control Center; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include "menu-browser.h"
#include "libslab-utils.h"

#define WINDOW_WIDTH_KEY  "/apps/application-browser/window_width"
#define WINDOW_HEIGHT_KEY "/apps/application-browser/window_height"
#define WINDOW_X_POS_KEY  "/apps/application-browser/window_position_x"
#define WINDOW_Y_POS_KEY  "/apps/application-browser/window_position_y"

static gboolean delete_event_cb (GtkWidget *, GdkEvent *, gpointer);
static gboolean destroy_cb      (GtkObject *, gpointer);

int
main (int argc, char **argv)
{
	GtkWindow *ab_window;
	GtkWidget *menu_browser;


#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init (
		"GNOME Application Browser", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL, NULL);

	menu_browser = menu_browser_new (gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_NONE));
	gtk_widget_show (menu_browser);

	ab_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_title (ab_window, _("Application Browser"));
	gtk_window_set_icon_name (ab_window, "gnome-fs-client");
	gtk_window_set_default_size (
		ab_window,
		GPOINTER_TO_INT (libslab_get_gconf_value (WINDOW_WIDTH_KEY)),
		GPOINTER_TO_INT (libslab_get_gconf_value (WINDOW_HEIGHT_KEY)));
	gtk_window_move (
		ab_window,
		GPOINTER_TO_INT (libslab_get_gconf_value (WINDOW_X_POS_KEY)),
		GPOINTER_TO_INT (libslab_get_gconf_value (WINDOW_Y_POS_KEY)));
	gtk_container_add (GTK_CONTAINER (ab_window), menu_browser);

	g_signal_connect (ab_window, "delete-event", G_CALLBACK (delete_event_cb), NULL);
	g_signal_connect (ab_window, "destroy",      G_CALLBACK (destroy_cb),      NULL);

	gtk_widget_show_all (GTK_WIDGET (ab_window));

	gtk_main ();

	return 0;
}

static gboolean
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gint x;
	gint y;


	gtk_window_get_position (GTK_WINDOW (widget), & x, & y);

	libslab_set_gconf_value (WINDOW_WIDTH_KEY,  GINT_TO_POINTER (widget->allocation.width));
	libslab_set_gconf_value (WINDOW_HEIGHT_KEY, GINT_TO_POINTER (widget->allocation.height));
	libslab_set_gconf_value (WINDOW_X_POS_KEY,  GINT_TO_POINTER (x));
	libslab_set_gconf_value (WINDOW_Y_POS_KEY,  GINT_TO_POINTER (y));

	gtk_widget_destroy (widget);

	return FALSE;
}

static gboolean
destroy_cb (GtkObject *object, gpointer data)
{
	gtk_main_quit ();

	return FALSE;
}
