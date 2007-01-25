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

#include "user-dirs-tile-table.h"

#include "directory-tile.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (UserDirsTileTable, user_dirs_tile_table, BOOKMARK_TILE_TABLE_TYPE)

static void update_store (LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

static GtkWidget *get_directory_tile (LibSlabBookmarkFile *, const gchar *);

GtkWidget *
user_dirs_tile_table_new ()
{
	GObject *this = g_object_new (USER_DIRS_TILE_TABLE_TYPE, "n-columns", 2, NULL);

	tile_table_reload (TILE_TABLE (this));

	return GTK_WIDGET (this);
}

static void
user_dirs_tile_table_class_init (UserDirsTileTableClass *this_class)
{
	BookmarkTileTableClass *bookmark_tile_table_class = BOOKMARK_TILE_TABLE_CLASS (this_class);

	bookmark_tile_table_class->get_store_path        = libslab_get_user_dirs_store_path;
	bookmark_tile_table_class->update_bookmark_store = update_store;
	bookmark_tile_table_class->get_tile              = get_directory_tile;
}

static void
user_dirs_tile_table_init (UserDirsTileTable *this)
{
	/* nothing to init */
}

static void
update_store (LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new,
              const gchar *uri)
{
	gchar    *title;
	gchar    *icon;
	gboolean  found;


	libslab_bookmark_file_set_mime_type   (bm_file_new, uri, "inode/directory");
	libslab_bookmark_file_add_application (bm_file_new, uri, "nautilus", "nautilus --browser %u");

	if (bm_file_old) {
		title = libslab_bookmark_file_get_title (bm_file_old, uri, NULL);
		libslab_bookmark_file_set_title (bm_file_new, uri, title);

		found = libslab_bookmark_file_get_icon (bm_file_old, uri, & icon, NULL, NULL);
		libslab_bookmark_file_set_icon (bm_file_new, uri, icon, NULL);

		g_free (title);
		g_free (icon);
	}
}

static GtkWidget *
get_directory_tile (LibSlabBookmarkFile *bm_file, const gchar *uri)
{
	gchar    *title;
	gchar    *icon;
	gboolean  found;

	GtkWidget *tile;


	if (bm_file) {
		title = libslab_bookmark_file_get_title (bm_file, uri, NULL);
		found = libslab_bookmark_file_get_icon (bm_file, uri, & icon, NULL, NULL);

		if (! found)
			icon = NULL;
	}

	tile = directory_tile_new (uri, title, icon);

	g_free (title);
	g_free (icon);

	return tile;
}
