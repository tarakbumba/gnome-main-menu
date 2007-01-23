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

#include "apps-agent.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs.h>

#include "libslab-utils.h"
#include "application-tile.h"
#include "system-tile.h"

G_DEFINE_TYPE (AppsAgent, apps_agent, G_TYPE_OBJECT)

typedef struct {
	TileTable *sys_table;
	TileTable *usr_table;
	TileTable *rct_table;

	gchar *sys_store_path;
	gchar *user_apps_store_path;

	GnomeVFSMonitorHandle *sys_store_monitor;
	GnomeVFSMonitorHandle *user_apps_store_monitor;
} AppsAgentPrivate;

typedef void (*CreateBookmarkEntryFunc) (
	LibSlabBookmarkFile *, LibSlabBookmarkFile *, const gchar *);

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), APPS_AGENT_TYPE, AppsAgentPrivate))

static void apps_agent_finalize (GObject *);

static void load_sys_table       (AppsAgent *);
static void load_user_apps_table (AppsAgent *);

static void update_sys_monitor       (AppsAgent *);
static void update_user_apps_monitor (AppsAgent *);

static void update_bookmark_store (const gchar *, const gchar *,
                                   GList *, CreateBookmarkEntryFunc);
static void add_bookmark_to_store (const gchar *, const gchar *,
                                   const gchar *, CreateBookmarkEntryFunc);

static void create_sys_item_bookmark_entry (LibSlabBookmarkFile *, LibSlabBookmarkFile *,
                                            const gchar *);
static void create_user_app_bookmark_entry (LibSlabBookmarkFile *, LibSlabBookmarkFile *,
                                            const gchar *);

static void system_table_update_cb    (TileTable *, TileTableUpdateEvent   *, gpointer);
static void system_table_uri_added_cb (TileTable *, TileTableURIAddedEvent *, gpointer);

static void user_apps_table_update_cb    (TileTable *, TileTableUpdateEvent *, gpointer);
static void user_apps_table_uri_added_cb (TileTable *, TileTableURIAddedEvent *, gpointer);

static void system_store_monitor_cb    (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                        GnomeVFSMonitorEventType, gpointer);
static void user_apps_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                        GnomeVFSMonitorEventType, gpointer);

static void tile_activated_cb (Tile *, TileEvent *, gpointer);

AppsAgent *
apps_agent_new (TileTable *system_table, TileTable *user_table, TileTable *recent_table)
{
	AppsAgent        *this;
	AppsAgentPrivate *priv;


	this = g_object_new (APPS_AGENT_TYPE, NULL);
	priv = PRIVATE (this);

	priv->sys_table = system_table;
	priv->usr_table = user_table;
	priv->rct_table = recent_table;

	g_object_ref (priv->sys_table);
	g_object_ref (priv->usr_table);
/*	g_object_ref (priv->rct_table); */

	g_signal_connect (
		G_OBJECT (priv->sys_table), TILE_TABLE_UPDATE_SIGNAL,
		G_CALLBACK (system_table_update_cb), NULL);

	g_signal_connect (
		G_OBJECT (priv->sys_table), TILE_TABLE_URI_ADDED_SIGNAL,
		G_CALLBACK (system_table_uri_added_cb), NULL);

	g_signal_connect (
		G_OBJECT (priv->usr_table), TILE_TABLE_UPDATE_SIGNAL,
		G_CALLBACK (user_apps_table_update_cb), NULL);

	g_signal_connect (
		G_OBJECT (priv->usr_table), TILE_TABLE_URI_ADDED_SIGNAL,
		G_CALLBACK (user_apps_table_uri_added_cb), NULL);

	load_sys_table       (this);
	load_user_apps_table (this);

	return this;
}

static void
apps_agent_class_init (AppsAgentClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = apps_agent_finalize;

	g_type_class_add_private (this_class, sizeof (AppsAgentPrivate));
}

static void
apps_agent_init (AppsAgent * this)
{
	AppsAgentPrivate *priv = PRIVATE (this);

	priv->sys_table = NULL;
	priv->usr_table = NULL;
	priv->rct_table = NULL;

	priv->sys_store_path       = NULL;
	priv->user_apps_store_path = NULL;

	priv->sys_store_monitor       = NULL;
	priv->user_apps_store_monitor = NULL;
}

static void
apps_agent_finalize (GObject *g_obj)
{
	AppsAgentPrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->sys_table);
	g_object_unref (priv->usr_table);
	g_object_unref (priv->rct_table);

	g_free (priv->sys_store_path);
	g_free (priv->user_apps_store_path);

	if (priv->sys_store_monitor)
		gnome_vfs_monitor_cancel (priv->sys_store_monitor);

	if (priv->user_apps_store_monitor)
		gnome_vfs_monitor_cancel (priv->user_apps_store_monitor);

	G_OBJECT_CLASS (apps_agent_parent_class)->finalize (g_obj);
}

static void
load_sys_table (AppsAgent *this)
{
	AppsAgentPrivate *priv = PRIVATE (this);

	gchar *path;
	LibSlabBookmarkFile *bm_file;
	gchar **uris = NULL;

	gchar     *title;
	GtkWidget *tile;

	GList *tiles = NULL;

	GError *error = NULL;

	gint i;


	path = libslab_get_system_item_store_path (FALSE);

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error) {
		uris = libslab_bookmark_file_get_uris (bm_file, NULL);

		for (i = 0; uris && uris [i]; ++i) {
			title = libslab_bookmark_file_get_title (bm_file, uris [i], NULL);
			tile  = system_tile_new (uris [i], title);

			if (tile) {
				g_signal_connect (
					G_OBJECT (tile), "tile-activated",
					G_CALLBACK (tile_activated_cb), NULL);

				tiles = g_list_append (tiles, tile);
			}
		}

		g_object_set (G_OBJECT (priv->sys_table), TILE_TABLE_TILES_PROP, tiles, NULL);
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't load bookmark file [%s]",
			__FUNCTION__, path);

	libslab_bookmark_file_free (bm_file);
	g_list_free (tiles);

	update_sys_monitor (this);
}

static void
load_user_apps_table (AppsAgent *this)
{
	AppsAgentPrivate *priv = PRIVATE (this);

	gchar *path;
	LibSlabBookmarkFile *bm_file;
	gchar **uris = NULL;

	GtkWidget *tile;

	GList *tiles = NULL;

	GError *error = NULL;

	gint i;


	path = libslab_get_user_apps_store_path (FALSE);

	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error) {
		uris = libslab_bookmark_file_get_uris (bm_file, NULL);

		for (i = 0; uris && uris [i]; ++i) {
			tile  = application_tile_new (uris [i]);

			if (tile) {
				g_signal_connect (
					G_OBJECT (tile), "tile-activated",
					G_CALLBACK (tile_activated_cb), NULL);

				tiles = g_list_append (tiles, tile);
			}
		}

		g_object_set (G_OBJECT (
			priv->usr_table), TILE_TABLE_TILES_PROP, tiles, NULL);
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't load bookmark file [%s]",
			__FUNCTION__, path);

	libslab_bookmark_file_free (bm_file);
	g_list_free (tiles);

	update_user_apps_monitor (this);
}

static void
update_sys_monitor (AppsAgent *this)
{
	AppsAgentPrivate *priv = PRIVATE (this);

	gchar *path;
	gchar *uri;


	path = libslab_get_system_item_store_path (FALSE);

	if (libslab_strcmp (path, priv->sys_store_path)) {
		g_free (priv->sys_store_path);
		priv->sys_store_path = path;

		uri = g_filename_to_uri (path, NULL, NULL);

		if (priv->sys_store_monitor)
			gnome_vfs_monitor_cancel (priv->sys_store_monitor);

		gnome_vfs_monitor_add (
			& priv->sys_store_monitor, uri, GNOME_VFS_MONITOR_FILE,
			system_store_monitor_cb, this);

		g_free (uri);
	}
}

static void
update_user_apps_monitor (AppsAgent *this)
{
	AppsAgentPrivate *priv = PRIVATE (this);

	gchar *path;
	gchar *uri;


	path = libslab_get_user_apps_store_path (FALSE);

	if (libslab_strcmp (path, priv->user_apps_store_path)) {
		g_free (priv->user_apps_store_path);
		priv->user_apps_store_path = path;

		uri = g_filename_to_uri (path, NULL, NULL);

		if (priv->user_apps_store_monitor)
			gnome_vfs_monitor_cancel (priv->user_apps_store_monitor);

		gnome_vfs_monitor_add (
			& priv->user_apps_store_monitor, uri, GNOME_VFS_MONITOR_FILE,
			user_apps_store_monitor_cb, this);

		g_free (uri);
	}
}

static void
system_table_update_cb (TileTable *table, TileTableUpdateEvent *event, gpointer user_data)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_system_item_store_path (FALSE);
	path_new = libslab_get_system_item_store_path (TRUE);

	update_bookmark_store (
		path_old, path_new, event->tiles_curr, create_sys_item_bookmark_entry);

	g_free (path_old);
	g_free (path_new);
}

static void
user_apps_table_update_cb (TileTable *table, TileTableUpdateEvent *event, gpointer user_data)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_user_apps_store_path (FALSE);
	path_new = libslab_get_user_apps_store_path (TRUE);

	update_bookmark_store (
		path_old, path_new, event->tiles_curr, create_user_app_bookmark_entry);

	g_free (path_old);
	g_free (path_new);
}

static void
update_bookmark_store (
	const gchar *path_old, const gchar *path_new,
	GList *tiles, CreateBookmarkEntryFunc func)
{
	LibSlabBookmarkFile *bm_file_old;
	LibSlabBookmarkFile *bm_file_new;

	GError *error = NULL;

	GList *node;


	bm_file_old = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file_old, path_old, & error);

	bm_file_new = libslab_bookmark_file_new ();

	for (node = tiles; node; node = node->next)
		func (bm_file_old, bm_file_new, TILE (node->data)->uri);

	if (error) {
		g_error_free (error);
		error = NULL;
	}

	libslab_bookmark_file_to_file (bm_file_new, path_new, & error);

	if (error)
		libslab_handle_g_error (
			& error,
			"%s: couldn't save bookmark file [%s]",
			__FUNCTION__, path_new);

	libslab_bookmark_file_free (bm_file_old);
	libslab_bookmark_file_free (bm_file_new);
}

static void
system_table_uri_added_cb (TileTable *table, TileTableURIAddedEvent *event, gpointer user_data)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_system_item_store_path (FALSE);
	path_new = libslab_get_system_item_store_path (TRUE);

	add_bookmark_to_store (
		path_old, path_new, event->uri, create_user_app_bookmark_entry);

	g_free (path_old);
	g_free (path_new);
}

static void
user_apps_table_uri_added_cb (TileTable *table, TileTableURIAddedEvent *event, gpointer user_data)
{
	gchar *path_old;
	gchar *path_new;


	path_old = libslab_get_user_apps_store_path (FALSE);
	path_new = libslab_get_user_apps_store_path (TRUE);

	add_bookmark_to_store (
		path_old, path_new, event->uri, create_user_app_bookmark_entry);

	g_free (path_old);
	g_free (path_new);
}

static void
add_bookmark_to_store (
	const gchar *path_old, const gchar *path_new,
	const gchar *uri, CreateBookmarkEntryFunc func)
{
	LibSlabBookmarkFile *bm_file;

	GError *error = NULL;


	bm_file = libslab_bookmark_file_new ();
	libslab_bookmark_file_load_from_file (bm_file, path_old, & error);

	if (! (error || libslab_bookmark_file_has_item (bm_file, uri))) {
		func (NULL, bm_file, uri);

		libslab_bookmark_file_to_file (bm_file, path_new, & error);

		if (error)
			libslab_handle_g_error (
				& error,
				"%s: couldn't save bookmark file [%s]",
				__FUNCTION__, path_new);
	}
	else if (error)
		libslab_handle_g_error (
			& error,
			"%s: couldn't open bookmark file [%s]",
			__FUNCTION__, path_old);
	else
		;

	libslab_bookmark_file_free (bm_file);
}

static void
system_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	load_sys_table (APPS_AGENT (user_data));
}

static void
user_apps_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	load_user_apps_table (APPS_AGENT (user_data));
}

static void
create_user_app_bookmark_entry (
	LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new, const gchar *uri)
{
	libslab_bookmark_file_set_mime_type   (bm_file_new, uri, "application/x-desktop");
	libslab_bookmark_file_add_application (bm_file_new, uri, NULL, NULL);
}

static void
create_sys_item_bookmark_entry (
	LibSlabBookmarkFile *bm_file_old, LibSlabBookmarkFile *bm_file_new, const gchar *uri)
{
	gchar *title = NULL;

	if (libslab_bookmark_file_has_item (bm_file_old, uri))
		title = libslab_bookmark_file_get_title (bm_file_old, uri, NULL);

	libslab_bookmark_file_set_mime_type (bm_file_new, uri, "application/x-desktop");
	libslab_bookmark_file_add_application (bm_file_new, uri, NULL, NULL);

	if (title)
		libslab_bookmark_file_set_title (bm_file_new, uri, title);

	g_free (title);
}

static void
tile_activated_cb (Tile *tile, TileEvent *event, gpointer user_data)
{
	if (event->type == TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		return;

	tile_trigger_action_with_time (tile, tile->default_action, event->time);
}
