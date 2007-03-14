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
#include "bookmark-agent.h"

G_DEFINE_TYPE (UserDirsTileTable, user_dirs_tile_table, TILE_TABLE_TYPE)

typedef struct {
	BookmarkAgent *agent;
} UserDirsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), USER_DIRS_TILE_TABLE_TYPE, UserDirsTileTablePrivate))

static void finalize     (GObject *);
static void reload_tiles (TileTable *);

static void agent_notify_cb (GObject *, GParamSpec *, gpointer);

GtkWidget *
user_dirs_tile_table_new ()
{
	UserDirsTileTable        *this;
	UserDirsTileTablePrivate *priv;
	
	
	this = g_object_new (
		USER_DIRS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_NONE,
		NULL);
	priv = PRIVATE (this);

	priv->agent = bookmark_agent_get_instance (BOOKMARK_STORE_USER_DIRS);

	tile_table_reload (TILE_TABLE (this));

	g_signal_connect (
		G_OBJECT (priv->agent), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
		G_CALLBACK (agent_notify_cb), this);

	return GTK_WIDGET (this);
}

static void
user_dirs_tile_table_class_init (UserDirsTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);

	g_obj_class->finalize = finalize;

	tile_table_class->reload = reload_tiles;

	g_type_class_add_private (this_class, sizeof (UserDirsTileTablePrivate));
}

static void
user_dirs_tile_table_init (UserDirsTileTable *this)
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
	UserDirsTileTablePrivate *priv = PRIVATE (this);

	BookmarkItem **items = NULL;
	GList         *tiles = NULL;
	GtkWidget     *tile;

	gint i;


	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

	for (i = 0; items && items [i]; ++i) {
		tile = directory_tile_new (items [i]->uri, items [i]->title, items [i]->icon);

		if (tile)
			tiles = g_list_append (tiles, tile);
	}

	g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);
}

static void
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
