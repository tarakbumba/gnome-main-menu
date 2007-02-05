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

#if GLIB_CHECK_VERSION (2, 12, 0)
#	define USE_G_BOOKMARK
#else
#	include "eggbookmarkfile.h"
#endif

#define DEFAULT_USER_XDG_DIR     ".local/share"
#define DEFAULT_GLOBAL_XDG_PATH  "/usr/local/share:/usr/share"
#define TOP_CONFIG_DIR           PACKAGE
#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"
#define APPS_BOOKMARK_FILENAME   "applications.xbel"
#define DOCS_BOOKMARK_FILENAME   "documents.xbel"
#define DIRS_BOOKMARK_FILENAME   "places.xbel"

#define SYSTEM_ITEM_GCONF_KEY    "/desktop/gnome/applications/main-menu/system-area/system_item_list"
#define HELP_ITEM_GCONF_KEY      "/desktop/gnome/applications/main-menu/system-area/help_item"
#define CC_ITEM_GCONF_KEY        "/desktop/gnome/applications/main-menu/system-area/control_center_item"
#define PM_ITEM_GCONF_KEY        "/desktop/gnome/applications/main-menu/system-area/package_manager_item"
#define LOCKSCREEN_GCONF_KEY     "/desktop/gnome/applications/main-menu/lock_screen_priority"
#define USER_APPS_GCONF_KEY      "/desktop/gnome/applications/main-menu/file-area/user_specified_apps"

#define LOGOUT_DESKTOP_ITEM      "gnome-session-logout.desktop"
#define SHUTDOWN_DESKTOP_ITEM    "gnome-session-shutdown.desktop"

static gboolean get_main_menu_user_data_file_path (gchar **, const gchar *, gboolean);

void
migrate_system_gconf_to_bookmark_file ()
{
	gchar *bookmark_path;
	gchar *bookmark_path_cp_dest;

	gchar *contents;

	gboolean need_migration = TRUE;

	GList *gconf_system_list;
	gint   system_tile_type;

	LibSlabBookmarkFile *bm_file;

	gchar **uris;

	gchar *path;

	GnomeDesktopItem *ditem;
	gchar            *ditem_id;
	const gchar      *loc;
	gchar            *uri;

	GList *screensavers;
	gchar *exec_string;
	gchar *cmd_string;
	gchar *arg_string;

	const gchar *name;

	gchar *data_dir;

	GError *error = NULL;

	GList *node_i;
	GList *node_j;

	gint i;


	need_migration = ! get_main_menu_user_data_file_path (& bookmark_path, SYSTEM_BOOKMARK_FILENAME, TRUE);

	if (! need_migration) {
		bm_file = libslab_bookmark_file_new ();

		libslab_bookmark_file_load_from_file (bm_file, bookmark_path, & error);

		if (error) {
			libslab_handle_g_error (
				& error, "%s: can't read system item store path [%s]\n",
				G_STRFUNC, bookmark_path);
		}
		else {
			uris = libslab_bookmark_file_get_uris (bm_file, NULL);

			for (i = 0; uris && uris [i]; ++i) {
				if (g_str_has_suffix (uris [i], "yelp.desktop"))
					libslab_bookmark_file_set_title (bm_file, uris [i], _("Help"));
				else if (g_str_has_suffix (uris [i], "gnome-session-logout.desktop"))
					libslab_bookmark_file_set_title (bm_file, uris [i], _("Logout"));
				else if (g_str_has_suffix (uris [i], "gnome-session-shutdown.desktop"))
					libslab_bookmark_file_set_title (bm_file, uris [i], _("Shutdown"));
				else
					/* do nothing */ ;
			}

			libslab_bookmark_file_to_file (bm_file, bookmark_path, & error);

			if (error)
				libslab_handle_g_error (
					& error, "%s: can't write system item store [%s]\n",
					G_STRFUNC, bookmark_path);

			g_strfreev (uris);
		}

		libslab_bookmark_file_free (bm_file);

		goto exit;
	}

	gconf_system_list = (GList *) libslab_get_gconf_value (SYSTEM_ITEM_GCONF_KEY);

	if (! gconf_system_list) {
		bookmark_path         = libslab_get_system_item_store_path (FALSE);
		bookmark_path_cp_dest = libslab_get_system_item_store_path (TRUE);

		g_file_get_contents (bookmark_path, & contents, NULL, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't read system item store path [%s]\n",
				G_STRFUNC, bookmark_path);
		else
			g_file_set_contents (bookmark_path_cp_dest, contents, -1, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't save system item store path [%s]\n",
				G_STRFUNC, bookmark_path_cp_dest);

		g_free (contents);
		g_free (bookmark_path_cp_dest);

		goto exit;
	}

	bm_file = libslab_bookmark_file_new ();

	get_main_menu_user_data_file_path (& data_dir, NULL, TRUE);

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
						data_dir, "lockscreen.desktop", NULL);

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
			libslab_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			libslab_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));

			name = gnome_desktop_item_get_string (ditem, GNOME_DESKTOP_ITEM_NAME);

			if (! strcmp (name, "Yelp"))
				libslab_bookmark_file_set_title (bm_file, uri, _("Help"));

			if (! strcmp (name, "Session Logout Dialog"))
				libslab_bookmark_file_set_title (bm_file, uri, _("Logout"));

			if (! strcmp (name, "System Shutdown Dialog"))
				libslab_bookmark_file_set_title (bm_file, uri, _("Shutdown"));
		}

		g_free (uri);
		g_free (ditem_id);

		if (ditem)
			gnome_desktop_item_unref (ditem);
	}

	libslab_bookmark_file_to_file (bm_file, bookmark_path, & error);

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: cannot save migrated system item list [%s]",
			G_STRFUNC, bookmark_path);

	libslab_bookmark_file_free (bm_file);

	g_list_free (gconf_system_list);
	g_free (data_dir);

exit:

	g_free (bookmark_path);
}

void
migrate_user_apps_gconf_to_bookmark_file ()
{
	gchar *bookmark_path;
	gchar *bookmark_path_cp_dest;

	gchar *contents;

	gboolean need_migration;

	GList *user_apps_list;

#ifdef USE_G_BOOKMARK
	GBookmarkFile *bm_file;
#else
	EggBookmarkFile *bm_file;
#endif

	GnomeDesktopItem *ditem;
	const gchar      *loc;
	gchar            *uri;

	GError *error = NULL;

	GList *node;


	need_migration = ! get_main_menu_user_data_file_path (& bookmark_path, APPS_BOOKMARK_FILENAME, TRUE);

	if (! need_migration)
		goto exit;

	user_apps_list = (GList *) libslab_get_gconf_value (USER_APPS_GCONF_KEY);

	if (! user_apps_list) {
		bookmark_path         = libslab_get_user_apps_store_path (FALSE);
		bookmark_path_cp_dest = libslab_get_user_apps_store_path (TRUE);

		g_file_get_contents (bookmark_path, & contents, NULL, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't read user apps store path [%s]\n",
				G_STRFUNC, bookmark_path);
		else
			g_file_set_contents (bookmark_path_cp_dest, contents, -1, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't save user apps store path [%s]\n",
				G_STRFUNC, bookmark_path_cp_dest);

		g_free (contents);
		g_free (bookmark_path_cp_dest);

		goto exit;
	}

#ifdef USE_G_BOOKMARK
	bm_file = g_bookmark_file_new ();
#else
	bm_file = egg_bookmark_file_new ();
#endif

	for (node = user_apps_list; node; node = node->next) {
		ditem = libslab_gnome_desktop_item_new_from_unknown_id ((gchar *) node->data);

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
#ifdef USE_G_BOOKMARK
			g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			g_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));
#else
			egg_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			egg_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));
#endif
		}

		g_free (uri);

		if (ditem)
			gnome_desktop_item_unref (ditem);
	}

#ifdef USE_G_BOOKMARK
	g_bookmark_file_to_file (bm_file, bookmark_path, & error);
#else
	egg_bookmark_file_to_file (bm_file, bookmark_path, & error);
#endif

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: cannot save migrated user apps list [%s]",
			G_STRFUNC, bookmark_path);

#ifdef USE_G_BOOKMARK
	g_bookmark_file_free (bm_file);
#else
	egg_bookmark_file_free (bm_file);
#endif

	g_list_free (user_apps_list);

exit:

	g_free (bookmark_path);
}

void
migrate_user_docs_to_user_bookmark_file ()
{
	gchar *bookmark_path;
	gchar *bookmark_path_cp_dest;

	gchar *contents;

	GError *error = NULL;


	if (! get_main_menu_user_data_file_path (& bookmark_path, DOCS_BOOKMARK_FILENAME, TRUE)) {
		g_free (bookmark_path);

		bookmark_path         = libslab_get_user_docs_store_path (FALSE);
		bookmark_path_cp_dest = libslab_get_user_docs_store_path (TRUE);

		g_file_get_contents (bookmark_path, & contents, NULL, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't read user docs store path [%s]\n",
				G_STRFUNC, bookmark_path);
		else
			g_file_set_contents (bookmark_path_cp_dest, contents, -1, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't save user docs store path [%s]\n",
				G_STRFUNC, bookmark_path_cp_dest);

		g_free (contents);
		g_free (bookmark_path_cp_dest);
	}

	g_free (bookmark_path);
}

void
migrate_user_dirs_to_user_bookmark_file ()
{
	LibSlabBookmarkFile *bm_file;

	gchar *bookmark_path;
	gchar *bookmark_path_cp_dest;

	gchar *contents;

	gchar **uris;
	gchar  *uri_new;
	gchar  *path;

	GError *error = NULL;

	gint i;


	if (! get_main_menu_user_data_file_path (& bookmark_path, DIRS_BOOKMARK_FILENAME, TRUE)) {
		g_free (bookmark_path);

		bookmark_path         = libslab_get_user_dirs_store_path (FALSE);
		bookmark_path_cp_dest = libslab_get_user_dirs_store_path (TRUE);

		g_file_get_contents (bookmark_path, & contents, NULL, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't read user dirs store path [%s]\n",
				G_STRFUNC, bookmark_path);
		else
			g_file_set_contents (bookmark_path_cp_dest, contents, -1, & error);

		if (error)
			libslab_handle_g_error (
				& error, "%s: can't save user dirs store path [%s]\n",
				G_STRFUNC, bookmark_path_cp_dest);

		g_free (contents);

		g_free (bookmark_path);
		bookmark_path = bookmark_path_cp_dest;

		bm_file = libslab_bookmark_file_new ();
		libslab_bookmark_file_load_from_file (bm_file, bookmark_path, & error);

		if (error) {
			libslab_handle_g_error (
				& error, "%s: can't read user dirs store path [%s]\n",
				G_STRFUNC, bookmark_path);
		}
		else {
			uris = libslab_bookmark_file_get_uris (bm_file, NULL);

			for (i = 0; uris && uris [i]; ++i) {
				if (! strcmp (uris [i], "HOME"))
					uri_new = g_filename_to_uri (g_get_home_dir (), NULL, NULL);
				else if (! strcmp (uris [i], "DESKTOP")) {
					path = g_build_filename (g_get_home_dir (), "Desktop", NULL);
					uri_new = g_filename_to_uri (path, NULL, NULL);
					g_free (path);
				}
				else if (! strcmp (uris [i], "SPREADSHEET")) {
					path = g_build_filename (g_get_home_dir (), "Documents", g_strconcat (_("New Spreadsheet"), ".ods", NULL), NULL);
					uri_new = g_filename_to_uri (path, NULL, NULL);
					g_free (path);
				}
				else if (! strcmp (uris [i], "DOCUMENT")) {
					path = g_build_filename (g_get_home_dir (), "Documents", g_strconcat (_("New Document"), ".odt", NULL), NULL);
					uri_new = g_filename_to_uri (path, NULL, NULL);
					g_free (path);
				}
				else
					uri_new = NULL;

				if (uri_new)
					libslab_bookmark_file_move_item (bm_file, uris [i], uri_new, NULL);

				g_free (uri_new);
			}

			libslab_bookmark_file_to_file (bm_file, bookmark_path, & error);

			if (error)
				libslab_handle_g_error (
					& error, "%s: can't write user dirs store [%s]\n",
					G_STRFUNC, bookmark_path);

			g_strfreev (uris);
		}

		libslab_bookmark_file_free (bm_file);
	}

	g_free (bookmark_path);
}

static gboolean
get_main_menu_user_data_file_path (gchar **path_out, const gchar *filename, gboolean user_only)
{
	GList *user_dirs   = NULL;
	GList *global_dirs = NULL;

	GList *potential_user_dirs = NULL;

	gchar **dirs;
	gchar  *path;
	gchar  *dir;

	gboolean need_mkdir;
	gboolean user_file_exists = FALSE;

	GList *node;
	gint   i;


	path = (gchar *) g_getenv ("XDG_DATA_HOME");

	if (path && g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		dir = g_build_filename (path, TOP_CONFIG_DIR, NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			user_dirs = g_list_append (user_dirs, dir);

		potential_user_dirs = g_list_append (potential_user_dirs, g_strdup (dir));
	}

	path = g_build_filename (g_get_home_dir (), DEFAULT_USER_XDG_DIR, NULL);
	dir  = g_build_filename (path, TOP_CONFIG_DIR, NULL);

	if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		user_dirs = g_list_append (user_dirs, dir);

	potential_user_dirs = g_list_append (potential_user_dirs, g_strdup (dir));

	g_free (path);

	if (! user_only) {
		dirs = g_strsplit (g_getenv ("XDG_DATA_DIRS"), ":", 0);

		if (! dirs [0]) {
			g_strfreev (dirs);
			dirs = g_strsplit (DEFAULT_GLOBAL_XDG_PATH, ":", 0);
		}

		for (i = 0; dirs [i]; ++i) {
			if (g_file_test (dirs [i], G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
				path = g_build_filename (dirs [i], TOP_CONFIG_DIR, NULL);
				global_dirs = g_list_append (global_dirs, path);
			}
		}

		g_strfreev (dirs);
	}

	path = NULL;

	for (node = user_dirs; ! path && node; node = node->next) {
		dir = (gchar *) node->data;

		if (filename) {
			path = g_build_filename (dir, filename, NULL);

			if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
				g_free (path);
				path = NULL;
			}
			else
				user_file_exists = TRUE;
		}
		else
			if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				path = g_strdup (dir);
	}

	for (node = global_dirs; ! path && node; node = node->next) {
		dir = (gchar *) node->data;

		if (filename) {
			path = g_build_filename (dir, filename, NULL);

			if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
				g_free (path);
				path = NULL;
			}
		}
		else
			if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				path = g_strdup (dir);
	}

	if (! path) {
		dir = NULL;

		need_mkdir = TRUE;

		for (node = user_dirs; need_mkdir && node; node = node->next) {
			dir = (gchar *) node->data;

			if (g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				need_mkdir = FALSE;
		}

		if (need_mkdir) {
			if (user_dirs)
				dir = (gchar *) user_dirs->data;
			else
				dir = (gchar *) potential_user_dirs->data;

			g_mkdir_with_parents (dir, 0700);
		}

		if (filename)
			path = g_build_filename (dir, filename, NULL);
		else
			path = g_strdup (dir);
	}

	for (node = user_dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (user_dirs);

	for (node = potential_user_dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (potential_user_dirs);

	for (node = global_dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (global_dirs);

	*path_out = path;

	return user_file_exists;
}
