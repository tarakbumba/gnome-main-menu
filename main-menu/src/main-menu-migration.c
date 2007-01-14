/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006, 2007 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "main-menu-migration.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>

#include "libslab-utils.h"

#define DEFAULT_USER_XDG_DIR     ".local/share"
#define TOP_CONFIG_DIR           PACKAGE
#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"

#define SYSTEM_ITEM_GCONF_KEY    "/desktop/gnome/applications/main-menu/system-area/system_item_list"
#define HELP_ITEM_GCONF_KEY      "/desktop/gnome/applications/main-menu/system-area/help_item"
#define CC_ITEM_GCONF_KEY        "/desktop/gnome/applications/main-menu/system-area/control_center_item"
#define PM_ITEM_GCONF_KEY        "/desktop/gnome/applications/main-menu/system-area/package_manager_item"
#define LOCKSCREEN_GCONF_KEY     "/desktop/gnome/applications/main-menu/lock_screen_priority"

#define LOGOUT_DESKTOP_ITEM      "gnome-session-logout.desktop"
#define SHUTDOWN_DESKTOP_ITEM    "gnome-session-shutdown.desktop"

static gchar *get_main_menu_user_data_path (void);

void
migrate_system_gconf_to_bookmark_file ()
{
	GList *user_dirs = NULL;
	gchar *path;

	gboolean need_migration = TRUE;

	GList *gconf_system_list;
	gint   system_tile_type;

	GBookmarkFile *bm_file;

	GnomeDesktopItem *ditem;
	gchar            *ditem_id;
	const gchar      *loc;
	gchar            *uri;

	GList *screensavers;
	gchar *exec_string;
	gchar *cmd_string;
	gchar *arg_string;

	gchar *data_dir;

	GError *error = NULL;

	GList *node_i;
	GList *node_j;

	gint i;


	path = g_build_filename (g_getenv ("XDG_DATA_HOME"), TOP_CONFIG_DIR, NULL);
	user_dirs = g_list_append (user_dirs, path);

	path = g_build_filename (
		g_get_home_dir (), DEFAULT_USER_XDG_DIR,
		TOP_CONFIG_DIR, NULL);
	user_dirs = g_list_append (user_dirs, path);


	path = g_build_filename (
		g_getenv ("XDG_DATA_HOME"), TOP_CONFIG_DIR,
		SYSTEM_BOOKMARK_FILENAME, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS))
		need_migration = FALSE;

	g_free (path);

	if (! need_migration)
		return;

	path = g_build_filename (
		g_get_home_dir (), DEFAULT_USER_XDG_DIR, TOP_CONFIG_DIR,
		SYSTEM_BOOKMARK_FILENAME, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS))
		need_migration = FALSE;

	g_free (path);

	if (! need_migration)
		return;

	gconf_system_list = (GList *) libslab_get_gconf_value (SYSTEM_ITEM_GCONF_KEY);

	if (! gconf_system_list)
		return;

	bm_file = g_bookmark_file_new ();

	data_dir = get_main_menu_user_data_path ();

	for (node_i = gconf_system_list; node_i; node_i = node_i->next) {
		system_tile_type = GPOINTER_TO_INT (node_i->data);

		if (system_tile_type == 0)
			ditem_id = (gchar *) libslab_get_gconf_value (HELP_ITEM_GCONF_KEY);
		else if (system_tile_type == 1)
			ditem_id = (gchar *) libslab_get_gconf_value (CC_ITEM_GCONF_KEY);
		else if (system_tile_type == 2)
			ditem_id = (gchar *) libslab_get_gconf_value (PM_ITEM_GCONF_KEY);
		else if (system_tile_type == 3) {
			screensavers = libslab_get_gconf_value (LOCKSCREEN_GCONF_KEY);

			for (node_j = screensavers; node_j; node_j = node_j->next) {
				exec_string = (gchar *) node_j->data;
	
				cmd_string = g_strdup (exec_string);
				arg_string = NULL;

				for (i = 0; i < strlen (exec_string); ++i) {
					if (g_ascii_isspace (exec_string [i])) {
						cmd_string = g_strndup (exec_string, i);
						arg_string = g_strdup (& exec_string [i + 1]);
					}
				}

				cmd_string = g_find_program_in_path (cmd_string);

				if (cmd_string) {
					ditem = gnome_desktop_item_new ();

					path = g_build_filename (
						data_dir, "gnome-main-menu-lockscreen.desktop", NULL);

					gnome_desktop_item_set_location_file (ditem, path);
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_NAME, _("Lock Screen"));
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_ICON, _("gnome-lockscreen"));
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_EXEC, exec_string);
					gnome_desktop_item_set_boolean (
						ditem, GNOME_DESKTOP_ITEM_TERMINAL, FALSE);
					gnome_desktop_item_set_entry_type (
						ditem, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_ENCODING, "UTF-8");
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_CATEGORIES, "GNOME;Application;");
					gnome_desktop_item_set_string (
						ditem, GNOME_DESKTOP_ITEM_ONLY_SHOW_IN, "GNOME;");

					gnome_desktop_item_save (ditem, NULL, TRUE, NULL);

					break;
				}

				g_free (cmd_string);
				g_free (arg_string);
				g_free (exec_string);
			}

			g_list_free (screensavers);

			ditem_id = path;
		}
		else if (system_tile_type == 4) {
			ditem_id = g_strdup (LOGOUT_DESKTOP_ITEM);

			node_i->data = GINT_TO_POINTER (5);

			node_i = node_i->prev;
		}
		else if (system_tile_type == 5)
			ditem_id = g_strdup (SHUTDOWN_DESKTOP_ITEM);
		else
			ditem_id = NULL;

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (ditem_id);

		if (ditem) {
			loc = gnome_desktop_item_get_location (ditem);

			if (g_path_is_absolute (loc))
				uri = g_filename_to_uri (loc, NULL, NULL);
			else
				uri = g_strdup (loc);
		}
		else
			uri = NULL;

		if (uri) {
			g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			g_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));
		}

		g_free (uri);
		g_free (ditem_id);

		if (ditem)
			gnome_desktop_item_unref (ditem);
	}

	path = g_build_filename (data_dir, SYSTEM_BOOKMARK_FILENAME, NULL);

	g_bookmark_file_to_file (bm_file, path, & error);

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: cannot save migrated system item list [%s]",
			G_GNUC_FUNCTION, path);

	g_bookmark_file_free (bm_file);
	g_list_free (gconf_system_list);
	g_free (data_dir);
	g_free (path);
}

static gchar *
get_main_menu_user_data_path ()
{
	GList *dirs = NULL;

	gchar *dir;
	gchar *path;

	GList *node;


	dir = g_strdup (g_getenv ("XDG_DATA_HOME"));
	if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		dirs = g_list_append (dirs, dir);

	dir = g_build_filename (g_get_home_dir (), DEFAULT_USER_XDG_DIR, NULL);
	if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		dirs = g_list_append (dirs, dir);

	path = NULL;

	for (node = dirs; ! path && node; node = node->next) {
		dir = (gchar *) node->data;
		path = g_build_filename ((gchar *) node->data, TOP_CONFIG_DIR, NULL);

		if (! g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_free (path);
			path = NULL;
		}
	}

	if (! path) {
		path = g_build_filename ((gchar *) dirs->data, TOP_CONFIG_DIR, NULL);

		g_mkdir_with_parents (path, 0700);
	}

	for (node = dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (dirs);

	return path;
}
