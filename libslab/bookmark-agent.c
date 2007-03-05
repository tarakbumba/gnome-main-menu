/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2007 Novell, Inc.
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

#include "bookmark-agent.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#else
#	define PACKAGE "gnome-main-menu"
#endif

#if ! GLIB_CHECK_VERSION (2, 12, 0)
#	include "bookmark-agent-egg.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs.h>

#include "libslab-utils.h"

#define MODIFIABLE_DOCS_GCONF_KEY "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_docs"

G_DEFINE_TYPE (BookmarkAgent, bookmark_agent, G_TYPE_OBJECT)

typedef struct {
	BookmarkItem         **items;
	gint                   n_items;
	BookmarkStoreStatus    status;

	GBookmarkFile         *store;

	gchar                 *store_path;
	gboolean               user_modifiable;

	GnomeVFSMonitorHandle *store_monitor;
	GnomeVFSMonitorHandle *user_store_monitor;
	guint                  gconf_monitor;
} BookmarkAgentPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BOOKMARK_AGENT_TYPE, BookmarkAgentPrivate))

enum {
	PROP_0,
	PROP_ITEMS,
	PROP_STATUS
};

enum {
	UPDATE_SIGNAL,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void get_property     (GObject *, guint, GValue *, GParamSpec *);
static void set_property     (GObject *, guint, const GValue *, GParamSpec *);
static void finalize         (GObject *);

static void   update_agent   (BookmarkAgent *);
static void   save_store     (BookmarkAgent *);
static gchar *get_store_path (BookmarkAgent *, gboolean);
static gchar *translate_uri  (BookmarkAgent *, const gchar *);
static void   free_item      (BookmarkItem *);

static void store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                              GnomeVFSMonitorEventType, gpointer);
static void gconf_notify_cb  (GConfClient *, guint, GConfEntry *, gpointer);

static BookmarkAgent *instance = NULL;

BookmarkAgent *
bookmark_agent_get_instance ()
{
	BookmarkAgentPrivate *priv;

	if (! instance) {
		instance = g_object_new (BOOKMARK_AGENT_TYPE, NULL);
		priv     = PRIVATE (instance);

		priv->store = g_bookmark_file_new ();
		priv->gconf_monitor = libslab_gconf_notify_add (
			MODIFIABLE_DOCS_GCONF_KEY, gconf_notify_cb, instance);

		update_agent (instance);
	}
	else
		g_object_ref (G_OBJECT (instance));

	return instance;
}

gboolean
bookmark_agent_has_item (BookmarkAgent *this, const gchar *uri)
{
	return g_bookmark_file_has_item (PRIVATE (this)->store, uri);
}

void
bookmark_agent_add_item (BookmarkAgent *this, const BookmarkItem *item)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	if (! item->uri || ! item->mime_type)
		return;

	g_bookmark_file_set_mime_type (priv->store, item->uri, item->mime_type);

	if (item->mtime)
		g_bookmark_file_set_modified (priv->store, item->uri, item->mtime);

	g_bookmark_file_add_application (priv->store, item->uri, item->app_name, item->app_exec);

	save_store (this);
}

void
bookmark_agent_remove_item (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	if (! uri)
		return;

	g_bookmark_file_remove_item (priv->store, uri, NULL);

	save_store (this);
}

void
bookmark_agent_reorder_items (BookmarkAgent *this, const gchar **uris)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar **groups;
	gchar  *group;

	gint i;
	gint j;


	for (i = 0; uris && uris [i]; ++i) {
		groups = g_bookmark_file_get_groups (priv->store, uris [i], NULL, NULL);

		for (j = 0; groups && groups [j]; ++j)
			if (g_str_has_prefix (groups [j], "rank-"))
				g_bookmark_file_remove_group (priv->store, uris [i], groups [j], NULL);

		g_strfreev (groups);

		group = g_strdup_printf ("rank-%d", i);
		g_bookmark_file_add_group (priv->store, uris [i], group);
		g_free (group);
	}

	save_store (this);
}

static void
bookmark_agent_class_init (BookmarkAgentClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *items_pspec;
	GParamSpec *status_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	items_pspec = g_param_spec_pointer (
		BOOKMARK_AGENT_ITEMS_PROP, BOOKMARK_AGENT_ITEMS_PROP,
		"the null-terminated list which contains the bookmark items in this store",
		G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	status_pspec = g_param_spec_int (
		BOOKMARK_AGENT_STORE_STATUS_PROP, BOOKMARK_AGENT_STORE_STATUS_PROP, "the status of the store",
		BOOKMARK_STORE_DEFAULT_ONLY, BOOKMARK_STORE_USER, BOOKMARK_STORE_DEFAULT,
		G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_ITEMS, items_pspec);
	g_object_class_install_property (g_obj_class, PROP_STATUS, status_pspec);

	signals [UPDATE_SIGNAL] = g_signal_new (
		BOOKMARK_AGENT_UPDATE_SIGNAL,
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (BookmarkAgentClass, update),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	g_type_class_add_private (this_class, sizeof (BookmarkAgentPrivate));
}

static void
bookmark_agent_init (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	priv->items              = NULL;
	priv->n_items            = 0;
	priv->status             = BOOKMARK_STORE_DEFAULT;

	priv->store              = NULL;

	priv->store_path         = NULL;
	priv->user_modifiable    = FALSE;

	priv->store_monitor      = NULL;
	priv->user_store_monitor = NULL;
	priv->gconf_monitor      = 0;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	BookmarkAgent        *this = BOOKMARK_AGENT (g_obj);
	BookmarkAgentPrivate *priv = PRIVATE        (this);


	switch (prop_id) {
		case PROP_ITEMS:
			g_value_set_pointer (value, priv->items);
			break;

		case PROP_STATUS:
			g_value_set_int (value, priv->status);
			break;

		default:
			break;
	}
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	/* no writeable properties */
}

static void
finalize (GObject *g_obj)
{
	BookmarkAgentPrivate *priv = PRIVATE (g_obj);

	gint i;


	for (i = 0; priv->items && priv->items [i]; ++i)
		free_item (priv->items [i]);

	g_free (priv->items);
	g_free (priv->store_path);

	if (priv->store_monitor)
		gnome_vfs_monitor_cancel (priv->store_monitor);

	if (priv->user_store_monitor)
		gnome_vfs_monitor_cancel (priv->user_store_monitor);

	libslab_gconf_notify_remove (priv->gconf_monitor);

	g_bookmark_file_free (priv->store);

	G_OBJECT_CLASS (bookmark_agent_parent_class)->finalize (g_obj);
}

static void
update_agent (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *store_path_new;

	gchar    **uris         = NULL;
	gchar    **uris_ordered = NULL;
	gsize      n_uris;
	gchar    **groups;
	gint       rank;
	gboolean   needs_update = FALSE;

	GError *error = NULL;

	gint i;
	gint j;


	store_path_new = get_store_path (this, FALSE);

	if (libslab_strcmp (priv->store_path, store_path_new)) {
		g_free (priv->store_path);
		priv->store_path = g_strdup (store_path_new);

		if (priv->store_monitor)
			gnome_vfs_monitor_cancel (priv->store_monitor);

		gnome_vfs_monitor_add (
			& priv->store_monitor, priv->store_path,
			GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}

	g_bookmark_file_load_from_file (priv->store, priv->store_path, & error);

	if (error)
		libslab_handle_g_error (
			& error, "%s: couldn't load bookmark file [%s]\n", G_STRFUNC, priv->store_path);

	uris         = g_bookmark_file_get_uris (priv->store, & n_uris);
	uris_ordered = g_new0 (gchar *, n_uris + 1);

	for (i = 0; uris && uris [i]; ++i) {
		groups = g_bookmark_file_get_groups (priv->store, uris [i], NULL, NULL);
		rank   = -1;

		for (j = 0; groups && groups [j]; ++j)
			if (g_str_has_prefix (groups [i], "rank-"))
				rank = atoi (& groups [i] [5]);

		g_strfreev (groups);

		if (rank < 0 || rank >= n_uris)
			rank = i;

		if (uris_ordered [rank])
			g_warning (
				"store corruption: [%s] rank = %d, [%s] [%s]",
				priv->store_path, rank, uris_ordered [rank], uris [i]);

		uris_ordered [rank] = translate_uri (this, uris [i]);
	}

	g_strfreev (uris);

	if (priv->n_items != n_uris)
		needs_update = TRUE;

	for (i = 0; ! needs_update && uris_ordered && uris_ordered [i]; ++i)
		if (strcmp (priv->items [i]->uri, uris_ordered [i]))
			needs_update = TRUE;

	if (needs_update) {
		for (i = 0; priv->items && priv->items [i]; ++i)
			free_item (priv->items [i]);

		g_free (priv->items);

		priv->n_items = n_uris;
		priv->items = g_new0 (BookmarkItem *, priv->n_items + 1);

		for (i = 0; uris_ordered && uris_ordered [i]; ++i) {
			priv->items [i]            = g_new0 (BookmarkItem, 1);
			priv->items [i]->uri       = g_strdup (uris_ordered [i]);
			priv->items [i]->title     = g_bookmark_file_get_title     (priv->store, priv->items [i]->uri, NULL);
			priv->items [i]->mime_type = g_bookmark_file_get_mime_type (priv->store, priv->items [i]->uri, NULL);
			priv->items [i]->mtime     = g_bookmark_file_get_modified  (priv->store, priv->items [i]->uri, NULL);
			priv->items [i]->app_name  = NULL;
			priv->items [i]->app_exec  = NULL;
		}

		g_strfreev (uris_ordered);

		g_signal_emit (this, signals [UPDATE_SIGNAL], 0);
	}
}

static void
save_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *store_path;

	GError *error = NULL;


	store_path = get_store_path (this, TRUE);

	if (! g_bookmark_file_to_file (priv->store, store_path, & error))
		libslab_handle_g_error (
			& error, "%s: couldn't save bookmark file [%s]\n", G_STRFUNC, store_path);

	g_free (store_path);
}

static gchar *
get_store_path (BookmarkAgent *this, gboolean writeable)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gboolean  sys_path;
	gchar    *path = NULL;
	gchar    *dir;
	gchar    *user_path;

	BookmarkStoreStatus status_new;

	const gchar * const *dirs = NULL;
	gint                 i;


	priv->user_modifiable = GPOINTER_TO_INT (
		libslab_get_gconf_value (MODIFIABLE_DOCS_GCONF_KEY));

	user_path = g_build_filename (g_get_user_data_dir (), PACKAGE, "documents.xbel", NULL);

	if (priv->user_modifiable) {
		path = g_strdup (user_path);

		if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
			if (writeable) {
				dir = g_path_get_dirname (path);
				g_mkdir_with_parents (dir, 0700);
				g_free (dir);
			}
			else {
				g_free (path);
				path = NULL;
			}
		}

		if (path)
			sys_path = FALSE;
	}

	if (! path && ! writeable) {
		dirs = g_get_system_data_dirs ();

		for (i = 0; ! path && dirs && dirs [i]; ++i) {
			path = g_build_filename (dirs [i], PACKAGE, "documents.xbel", NULL);

			if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
				g_free (path);
				path = NULL;
			}
		}

		sys_path = TRUE;
	}

	if (! priv->user_modifiable)
		status_new = BOOKMARK_STORE_DEFAULT_ONLY;
	else if (! sys_path)
		status_new = BOOKMARK_STORE_USER;
	else
		status_new = BOOKMARK_STORE_DEFAULT;

	if (priv->status != status_new) {
		priv->status = status_new;
		g_object_notify (G_OBJECT (this), BOOKMARK_AGENT_STORE_STATUS_PROP);

		if (priv->user_store_monitor)
			gnome_vfs_monitor_cancel (priv->user_store_monitor);

		if (priv->status == BOOKMARK_STORE_DEFAULT)
			gnome_vfs_monitor_add (
				& priv->store_monitor, user_path,
				GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}

	g_free (user_path);

	return path;
}

static gchar *
translate_uri (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *uri_new;
	gchar *path;
	gchar *dir;
	gchar *file;


	if (! strcmp (uri, "BLANK_SPREADSHEET") || ! strcmp (uri, "BLANK_DOCUMENT")) {
		dir = g_build_filename (g_get_home_dir (), "Documents", NULL);

		if (! strcmp (uri, "BLANK_SPREADSHEET"))
			file = g_strconcat (_("New Spreadsheet"), ".ods", NULL);
		else
			file = g_strconcat (_("New Document"), ".odt", NULL);

		path = g_build_filename (dir, file, NULL);

		if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_mkdir_with_parents (dir, 0700);
			fclose (g_fopen (path, "w"));
		}

		uri_new = g_filename_to_uri (path, NULL, NULL);
		g_bookmark_file_move_item (priv->store, uri, uri_new, NULL);

		g_free (dir);
		g_free (file);
		g_free (path);
	}
	else
		uri_new = g_strdup (uri);

	return uri_new;
}

static void
free_item (BookmarkItem *item)
{
	g_free (item->uri);
	g_free (item->title);
	g_free (item->mime_type);
	g_free (item->app_name);
	g_free (item->app_exec);
	g_free (item);
}

static void
store_monitor_cb (GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
                  const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	update_agent (BOOKMARK_AGENT (user_data));
}

static void
gconf_notify_cb (GConfClient *client, guint conn_id,
                 GConfEntry *entry, gpointer user_data)
{
	BookmarkAgent        *this = BOOKMARK_AGENT (user_data);
	BookmarkAgentPrivate *priv = PRIVATE        (this);

	gboolean user_modifiable;


	user_modifiable = GPOINTER_TO_INT (libslab_get_gconf_value (MODIFIABLE_DOCS_GCONF_KEY));

	if (priv->user_modifiable != user_modifiable) {
		priv->user_modifiable = user_modifiable;
		update_agent (this);
	}
}
