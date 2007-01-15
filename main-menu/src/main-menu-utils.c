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

#include "main-menu-utils.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "system-tile.h"
#include "libslab-utils.h"

#include "main-menu-migration.h"

#define GLOBAL_XDG_PATH_ENV_VAR  "XDG_DATA_DIRS"
#define DEFAULT_GLOBAL_XDG_PATH  "/usr/local/share:/usr/share"
#define DEFAULT_USER_XDG_DIR     ".local/share"
#define TOP_CONFIG_DIR           PACKAGE

#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"
#define APPS_BOOKMARK_FILENAME   "applications.xbel"
#define DOCS_BOOKMARK_FILENAME   "documents.xbel"
#define DIRS_BOOKMARK_FILENAME   "places.xbel"

static gchar                 *get_data_file_path     (const gchar *);
static GList                 *get_uri_list           (const gchar *);
static void                   save_uri_list          (const gchar *, const GList *);
static GnomeVFSMonitorHandle *add_store_file_monitor (const gchar *,
                                                      GnomeVFSMonitorCallback,
                                                      gpointer);

GList *
get_system_item_uris ()
{
	migrate_system_gconf_to_bookmark_file ();

	return get_uri_list (SYSTEM_BOOKMARK_FILENAME);
}

GList *
get_app_uris ()
{
	migrate_user_apps_gconf_to_bookmark_file ();

	return get_uri_list (APPS_BOOKMARK_FILENAME);
}

void
save_system_item_uris (const GList *uris)
{
	save_uri_list (SYSTEM_BOOKMARK_FILENAME, uris);
}

void
save_app_uris (const GList *uris)
{
	save_uri_list (APPS_BOOKMARK_FILENAME, uris);
}

GnomeVFSMonitorHandle *
add_system_item_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	migrate_system_gconf_to_bookmark_file ();

	return add_store_file_monitor (SYSTEM_BOOKMARK_FILENAME, callback, user_data);
}

GnomeVFSMonitorHandle *
add_apps_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	migrate_user_apps_gconf_to_bookmark_file ();

	return add_store_file_monitor (APPS_BOOKMARK_FILENAME, callback, user_data);
}

static GList *
get_uri_list (const gchar *filename)
{
	GBookmarkFile *bm_file;
	gchar         *path;

	gchar **uris_array;
	GList  *uris_list = NULL;

	GError *error = NULL;

	gint i;


	path = get_data_file_path (filename);

	if (! path)
		return NULL;

	bm_file = g_bookmark_file_new ();
	g_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error) {
		uris_array = g_bookmark_file_get_uris (bm_file, NULL);

		for (i = 0; uris_array [i]; ++i)
			uris_list = g_list_append (uris_list, g_strdup (uris_array [i]));
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't load bookmark file [%s]",
			G_GNUC_FUNCTION, path);

	g_free (path);
	g_strfreev (uris_array);
	g_bookmark_file_free (bm_file);

	return uris_list;
}

static void
save_uri_list (const gchar *filename, const GList *uris)
{
	GBookmarkFile *bm_file;
	gchar         *path;

	gchar *uri;

	GnomeDesktopItem *ditem;

	const GList *node;

	GError *error = NULL;


	path = get_data_file_path (filename);

	if (! path)
		return;

	bm_file = g_bookmark_file_new ();

	for (node = uris; node; node = node->next) {
		uri = (gchar *) node->data;

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (uri);

		if (ditem) {
			g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			g_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));

			gnome_desktop_item_unref (ditem);
		}
	}

	g_bookmark_file_to_file (bm_file, path, & error);

	if (error)
		libslab_handle_g_error (
			& error, "%s: cannot save system item list [%s]",
			G_GNUC_FUNCTION, path);

	g_bookmark_file_free (bm_file);
}

static gchar *
get_data_file_path (const gchar *filename)
{
	GList *dirs = NULL;
	gchar *path = NULL;

	gchar **global_dirs;
	gchar  *path_i;

	GList *node;
	gint   i;


	path_i = g_build_filename (
		g_getenv ("XDG_DATA_HOME"), TOP_CONFIG_DIR, NULL);
	dirs = g_list_append (dirs, path_i);

	path_i = g_build_filename (
		g_get_home_dir (), DEFAULT_USER_XDG_DIR,
		TOP_CONFIG_DIR, NULL);
	dirs = g_list_append (dirs, path_i);

	global_dirs = g_strsplit (g_getenv ("XDG_DATA_DIRS"), ":", 0);

	if (! global_dirs [0]) {
		g_strfreev (global_dirs);
		global_dirs = g_strsplit (DEFAULT_GLOBAL_XDG_PATH, ":", 0);
	}

	for (i = 0; global_dirs [i]; ++i) {
		path_i = g_build_filename (
			global_dirs [i], TOP_CONFIG_DIR, NULL);
		dirs = g_list_append (dirs, path_i);
	}

	g_strfreev (global_dirs);

	for (node = dirs; ! path && node; node = node->next) {
		path = g_build_filename (
			(gchar *) node->data, filename, NULL);

		if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_free (path);
			path = NULL;
		}
	}

	for (node = dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (dirs);

	return path;
}

static GnomeVFSMonitorHandle *
add_store_file_monitor (const gchar *filename, GnomeVFSMonitorCallback callback, gpointer user_data)
{
	GnomeVFSMonitorHandle *handle;
	gchar                 *path;
	gchar                 *uri;


	path = get_data_file_path (filename);
	uri  = g_filename_to_uri (path, NULL, NULL);

	gnome_vfs_monitor_add (& handle, uri, GNOME_VFS_MONITOR_FILE, callback, user_data);

	g_free (path);
	g_free (uri);

	return handle;
}
