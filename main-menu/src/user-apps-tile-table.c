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

#include "user-apps-tile-table.h"

#include <string.h>

#include "application-tile.h"
#include "libslab-utils.h"
#include "bookmark-agent.h"

#define DISABLE_TERMINAL_GCONF_KEY "/desktop/gnome/lockdown/disable_command_line"

G_DEFINE_TYPE (UserAppsTileTable, user_apps_tile_table, TILE_TABLE_TYPE)

typedef struct {
	BookmarkAgent *agent;

	gulong agent_notify_handler_id;
} UserAppsTileTablePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), USER_APPS_TILE_TABLE_TYPE, UserAppsTileTablePrivate))

static void finalize     (GObject *);
static void reload_tiles (TileTable *);
static void reorder      (TileTable *, TileTableReorderEvent  *);
static void uri_added    (TileTable *, TileTableURIAddedEvent *);

static void agent_notify_cb (GObject *, GParamSpec *, gpointer);

GtkWidget *
user_apps_tile_table_new ()
{
	UserAppsTileTable        *this;
	UserAppsTileTablePrivate *priv;
	
	
	this = g_object_new (
		USER_APPS_TILE_TABLE_TYPE,
		"n-columns",             2,
		"homogeneous",           TRUE,
		TILE_TABLE_REORDER_PROP, TILE_TABLE_REORDERING_PUSH_PULL,
		NULL);
	priv = PRIVATE (this);

	priv->agent = bookmark_agent_get_instance (BOOKMARK_STORE_USER_APPS);

	tile_table_reload (TILE_TABLE (this));

	g_signal_connect (
		G_OBJECT (priv->agent), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
		G_CALLBACK (agent_notify_cb), this);

	return GTK_WIDGET (this);
}

static void
user_apps_tile_table_class_init (UserAppsTileTableClass *this_class)
{
	GObjectClass   *g_obj_class      = G_OBJECT_CLASS   (this_class);
	TileTableClass *tile_table_class = TILE_TABLE_CLASS (this_class);

	g_obj_class->finalize = finalize;

	tile_table_class->reload    = reload_tiles;
	tile_table_class->reorder   = reorder;
	tile_table_class->uri_added = uri_added;

	g_type_class_add_private (this_class, sizeof (UserAppsTileTablePrivate));
}

static void
user_apps_tile_table_init (UserAppsTileTable *this)
{
	UserAppsTileTablePrivate *priv = PRIVATE (this);

	priv->agent                   = NULL;
	priv->agent_notify_handler_id = 0;
}

static void
finalize (GObject *g_obj)
{
	UserAppsTileTablePrivate *priv = PRIVATE (g_obj);

	if (priv->agent_notify_handler_id)
		g_signal_handler_disconnect (priv->agent, priv->agent_notify_handler_id);

	g_object_unref (G_OBJECT (priv->agent));
}

static void
reload_tiles (TileTable *this)
{
	UserAppsTileTablePrivate *priv = PRIVATE (this);

	BookmarkItem **items = NULL;
	GList         *tiles = NULL;
	GtkWidget     *tile;

	gint i;


	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_ITEMS_PROP, & items, NULL);

	for (i = 0; items && items [i]; ++i) {
		tile = application_tile_new (items [i]->uri);

		if (tile)
			tiles = g_list_append (tiles, tile);
	}

	g_object_set (G_OBJECT (this), TILE_TABLE_TILES_PROP, tiles, NULL);
}

static void
reorder (TileTable *this, TileTableReorderEvent *event)
{
	UserAppsTileTablePrivate *priv = PRIVATE (this);

	gchar **uris;
	GList  *node;
	gint    i;


	uris = g_new0 (gchar *, g_list_length (event->tiles) + 1);

	for (node = event->tiles, i = 0; node; node = node->next, ++i)
		uris [i] = g_strdup (TILE (node->data)->uri);

	bookmark_agent_reorder_items (priv->agent, (const gchar **) uris);

	g_strfreev (uris);
}

static void
uri_added (TileTable *this, TileTableURIAddedEvent *event)
{
	UserAppsTileTablePrivate *priv = PRIVATE (this);

	BookmarkItem item;

	gboolean ineligible;
	gint     uri_len;


	if (! event->uri)
		return;

	item.uri = event->uri;

	ineligible = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));
	ineligible = ineligible && libslab_desktop_item_is_a_terminal (item.uri);

	if (ineligible)
		return;

	uri_len = strlen (item.uri);

	if (! strcmp (& item.uri [uri_len - 8], ".desktop")) {
		item.mime_type = "application/x-desktop";
		item.mtime     = 0;
		item.app_name  = NULL;
		item.app_exec  = NULL;

		bookmark_agent_add_item (priv->agent, & item);
	}
}

static void
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	reload_tiles (TILE_TABLE (user_data));
}
