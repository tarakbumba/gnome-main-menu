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

#include "user-docs-tile-table.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "document-tile.h"
#include "bookmark-agent.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (UserDocsTileTable, user_docs_tile_table, TILE_TABLE_TYPE)

typedef struct {
	BookmarkAgent *agent;
} UserDocsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), USER_DOCS_TILE_TABLE_TYPE, UserDocsTileTablePrivate))

static void finalize     (GObject *);
static void reload_tiles (TileTable *);
static void reorder      (TileTable *, TileTableReorderEvent  *);
static void uri_added    (TileTable *, TileTableURIAddedEvent *);

static void agent_update_cb (BookmarkAgent *, gpointer);

GtkWidget *
user_docs_tile_table_new ()
{
	UserDocsTileTable        *this;
	UserDocsTileTablePrivate *priv;
	
	
	this = g_object_new (
		USER_DOCS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_PUSH_PULL,
		NULL);
	priv = PRIVATE (this);

	priv->agent = bookmark_agent_get_instance ();

	tile_table_reload (TILE_TABLE (this));

	g_signal_connect (
		G_OBJECT (priv->agent), BOOKMARK_AGENT_UPDATE_SIGNAL,
		G_CALLBACK (agent_update_cb), this);

	return GTK_WIDGET (this);
}

static void
user_docs_tile_table_class_init (UserDocsTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);

	g_obj_class->finalize = finalize;

	tile_table_class->reload    = reload_tiles;
	tile_table_class->reorder   = reorder;
	tile_table_class->uri_added = uri_added;

	g_type_class_add_private (this_class, sizeof (UserDocsTileTablePrivate));
}

static void
user_docs_tile_table_init (UserDocsTileTable *this)
{
	PRIVATE (this)->agent = NULL;
}

static void
finalize (GObject *g_obj)
{
	g_object_unref (G_OBJECT (PRIVATE (g_obj)->agent));
}

static void
reload_tiles (TileTable *this)
{
	UserDocsTileTablePrivate *priv = PRIVATE (this);

	BookmarkItem **items = NULL;
	GList         *tiles = NULL;
	GtkWidget     *tile;

	gint i;


	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

	for (i = 0; items && items [i]; ++i) {
		tile = document_tile_new (items [i]->uri, items [i]->mime_type, items [i]->mtime);

		if (tile)
			tiles = g_list_append (tiles, tile);
	}

	g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);
}

static void
reorder (TileTable *this, TileTableReorderEvent *event)
{
	UserDocsTileTablePrivate *priv = PRIVATE (this);

	gchar **uris;
	GList  *node;
	gint    i;


	uris = g_new0 (gchar *, g_list_length (event->tiles));

	for (node = event->tiles, i = 0; node; node = node->next, ++i)
		uris [i] = g_strdup (TILE (node->data)->uri);

	bookmark_agent_reorder_items (priv->agent, (const gchar **) uris);

	g_strfreev (uris);
}

static void
uri_added (TileTable *this, TileTableURIAddedEvent *event)
{
	UserDocsTileTablePrivate *priv = PRIVATE (this);

	BookmarkItem item;

	GnomeVFSFileInfo        *info;
	GnomeVFSMimeApplication *default_app;


	item.uri = event->uri;

	info = gnome_vfs_file_info_new ();

	gnome_vfs_get_file_info (
		item.uri, info, 
		GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_FAST_MIME_TYPE);

	item.mime_type = g_strdup (info->mime_type);
	item.mtime     = info->mtime;

	if (item.mime_type) {
		default_app = gnome_vfs_mime_get_default_application (item.mime_type);

		item.app_name = g_strdup (gnome_vfs_mime_application_get_name (default_app));
		item.app_exec = g_strdup (gnome_vfs_mime_application_get_exec (default_app));

		gnome_vfs_mime_application_free (default_app);

		bookmark_agent_add_item (priv->agent, & item);

		g_free (item.mime_type);
		g_free (item.app_name);
		g_free (item.app_exec);
	}

	gnome_vfs_file_info_unref (info);
}

static void
agent_update_cb (BookmarkAgent *agent, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
