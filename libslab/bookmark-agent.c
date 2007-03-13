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

#define MODIFIABLE_APPS_GCONF_KEY "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_apps"
#define MODIFIABLE_DOCS_GCONF_KEY "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_docs"
#define MODIFIABLE_DIRS_GCONF_KEY "/desktop/gnome/applications/main-menu/lock-down/user_modifiable_dirs"

#define USER_APPS_STORE_FILE_NAME "applications.xbel"
#define USER_DOCS_STORE_FILE_NAME "documents.xbel"
#define USER_DIRS_STORE_FILE_NAME "places.xbel"

#define GTK_BOOKMARKS_FILE ".gtk-bookmarks"

G_DEFINE_TYPE (BookmarkAgent, bookmark_agent, G_TYPE_OBJECT)

typedef struct {
	BookmarkStoreType          type;

	BookmarkItem             **items;
	gint                       n_items;
	BookmarkStoreStatus        status;

	GBookmarkFile             *store;
	gboolean                   needs_sync;

	gchar                     *store_path;
	gchar                     *user_store_path;
	gboolean                   user_modifiable;
	gboolean                   reorderable;
	gchar                     *store_filename;
	gchar                     *lockdown_key;

	GnomeVFSMonitorHandle     *store_monitor;
	GnomeVFSMonitorHandle     *user_store_monitor;
	GnomeVFSMonitorHandle     *gtk_store_monitor;
	guint                      gconf_monitor;

	void                    (* update_path)   (BookmarkAgent *);
	void                    (* load_store)    (BookmarkAgent *);
	void                    (* save_store)    (BookmarkAgent *);
	gchar                 * (* translate_uri) (BookmarkAgent *, const gchar *);
} BookmarkAgentPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BOOKMARK_AGENT_TYPE, BookmarkAgentPrivate))

enum {
	PROP_0,
	PROP_ITEMS,
	PROP_STATUS
};

static BookmarkAgent *instances [BOOKMARK_STORE_N_TYPES];

static BookmarkAgent *bookmark_agent_new (BookmarkStoreType);

static void get_property     (GObject *, guint, GValue *, GParamSpec *);
static void set_property     (GObject *, guint, const GValue *, GParamSpec *);
static void finalize         (GObject *);

static void   update_agent  (BookmarkAgent *);
static gint   get_rank      (BookmarkAgent *, const gchar *);
static void   set_rank      (BookmarkAgent *, const gchar *, gint);
static void   save_store    (BookmarkAgent *);
static gchar *translate_uri (BookmarkAgent *, const gchar *);
static void   free_item     (BookmarkItem *);

static void   load_xbel_store       (BookmarkAgent *);
static void   update_user_spec_path (BookmarkAgent *);
static void   save_xbel_store       (BookmarkAgent *);
static gchar *translate_app_uri     (BookmarkAgent *, const gchar *);
static gchar *translate_doc_uri     (BookmarkAgent *, const gchar *);
static gchar *translate_dir_uri     (BookmarkAgent *, const gchar *);

static void store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                              GnomeVFSMonitorEventType, gpointer);
static void gconf_notify_cb  (GConfClient *, guint, GConfEntry *, gpointer);




BookmarkAgent *
bookmark_agent_get_instance (BookmarkStoreType type)
{
	g_return_val_if_fail (0 <= type, NULL);
	g_return_val_if_fail (type < BOOKMARK_STORE_N_TYPES, NULL);

	if (! instances [type])
		instances [type] = bookmark_agent_new (type);
	else
		g_object_ref (G_OBJECT (instances [type]));

	return instances [type];
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

	g_return_if_fail (priv->user_modifiable);
	g_return_if_fail (item->uri);
	g_return_if_fail (item->mime_type);

	g_bookmark_file_set_mime_type (priv->store, item->uri, item->mime_type);

	if (item->mtime)
		g_bookmark_file_set_modified (priv->store, item->uri, item->mtime);

	g_bookmark_file_add_application (priv->store, item->uri, item->app_name, item->app_exec);

	set_rank (this, item->uri, g_bookmark_file_get_size (priv->store) - 1);

	save_store (this);
}

void
bookmark_agent_remove_item (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gint rank;

	gchar **uris = NULL;
	gint    rank_i;
	gint    i;


	g_return_if_fail (priv->user_modifiable);

	if (! bookmark_agent_has_item (this, uri))
		return;

	rank = get_rank (this, uri);

	g_bookmark_file_remove_item (priv->store, uri, NULL);

	if (rank >= 0) {
		uris = g_bookmark_file_get_uris (priv->store, NULL);
                         
		for (i =  0; uris && uris [i]; ++i) {
			 rank_i = get_rank (this, uris [i]);

			if (rank_i > rank)
				set_rank (this, uris [i], rank_i - 1);
		}
	}

	save_store (this);
}

void
bookmark_agent_reorder_items (BookmarkAgent *this, const gchar **uris)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gint i;


	g_return_if_fail (priv->reorderable);

	for (i = 0; uris && uris [i]; ++i)
		set_rank (this, uris [i], i);

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

	g_object_class_install_property (g_obj_class, PROP_ITEMS,  items_pspec);
	g_object_class_install_property (g_obj_class, PROP_STATUS, status_pspec);

	g_type_class_add_private (this_class, sizeof (BookmarkAgentPrivate));
}

static void
bookmark_agent_init (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	priv->type               = -1;

	priv->items              = NULL;
	priv->n_items            = 0;
	priv->status             = BOOKMARK_STORE_ABSENT;

	priv->store              = NULL;
	priv->needs_sync         = FALSE;

	priv->store_path         = NULL;
	priv->user_store_path    = NULL;
	priv->user_modifiable    = FALSE;
	priv->reorderable        = FALSE;
	priv->store_filename     = NULL;
	priv->lockdown_key       = NULL;

	priv->store_monitor      = NULL;
	priv->user_store_monitor = NULL;
	priv->gtk_store_monitor  = NULL;
	priv->gconf_monitor      = 0;

	priv->update_path        = NULL;
	priv->load_store         = NULL;
	priv->save_store         = NULL;
}

static BookmarkAgent *
bookmark_agent_new (BookmarkStoreType type)
{
	BookmarkAgent        *this;
	BookmarkAgentPrivate *priv;

	this = g_object_new (BOOKMARK_AGENT_TYPE, NULL);
	priv = PRIVATE (this);

	priv->type  = type;
	priv->store = g_bookmark_file_new ();

	switch (type) {
		case BOOKMARK_STORE_USER_APPS:
			priv->lockdown_key   = MODIFIABLE_APPS_GCONF_KEY;
			priv->store_filename = USER_APPS_STORE_FILE_NAME;
			priv->translate_uri  = translate_app_uri;
			break;

		case BOOKMARK_STORE_USER_DOCS:
			priv->lockdown_key   = MODIFIABLE_DOCS_GCONF_KEY;
			priv->store_filename = USER_DOCS_STORE_FILE_NAME;
			priv->translate_uri  = translate_doc_uri;
			break;

		case BOOKMARK_STORE_USER_DIRS:
			priv->lockdown_key   = MODIFIABLE_DIRS_GCONF_KEY;
			priv->store_filename = USER_DIRS_STORE_FILE_NAME;
			priv->translate_uri  = translate_dir_uri;

			priv->user_modifiable = GPOINTER_TO_INT (libslab_get_gconf_value (priv->lockdown_key));
			priv->reorderable     = FALSE;

			break;

		default:
			break;
	}

	if (type == BOOKMARK_STORE_USER_APPS || type == BOOKMARK_STORE_USER_DOCS || type == BOOKMARK_STORE_USER_DIRS) {
		priv->user_modifiable = GPOINTER_TO_INT (libslab_get_gconf_value (priv->lockdown_key));

		priv->user_store_path = g_build_filename (
			g_get_user_data_dir (), PACKAGE, priv->store_filename, NULL);

		priv->update_path = update_user_spec_path;

		priv->gconf_monitor = libslab_gconf_notify_add (
			priv->lockdown_key, gconf_notify_cb, this);
	}

	if (type == BOOKMARK_STORE_USER_APPS || type == BOOKMARK_STORE_USER_DOCS) {
		priv->reorderable = TRUE;
		priv->load_store  = load_xbel_store;
		priv->save_store  = save_xbel_store;
	}

	update_agent (this);

	return this;
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
	g_free (priv->user_store_path);

	if (priv->store_monitor)
		gnome_vfs_monitor_cancel (priv->store_monitor);

	if (priv->user_store_monitor)
		gnome_vfs_monitor_cancel (priv->user_store_monitor);

	if (priv->gtk_store_monitor)
		gnome_vfs_monitor_cancel (priv->gtk_store_monitor);

	libslab_gconf_notify_remove (priv->gconf_monitor);

	g_bookmark_file_free (priv->store);

	G_OBJECT_CLASS (bookmark_agent_parent_class)->finalize (g_obj);
}

static void
update_agent (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	priv->update_path (this);
	priv->load_store  (this);
}

static gint
get_rank (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar **groups;
	gint    rank;

	gint i;


	if (! priv->reorderable)
		return -1;

	groups = g_bookmark_file_get_groups (priv->store, uri, NULL, NULL);
	rank   = -1;

	for (i = 0; groups && groups [i]; ++i) {
		if (g_str_has_prefix (groups [i], "rank-")) {
			if (rank >= 0)
				g_warning (
					"store corruption - multiple ranks for same uri: [%s] [%s]",
					priv->store_path, uri);

			rank = atoi (& groups [i] [5]);
		}
	}

	g_strfreev (groups);

	return rank;
}

static void
set_rank (BookmarkAgent *this, const gchar *uri, gint rank)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar **groups;
	gchar  *group;

	gint i;


	if (! (priv->reorderable && bookmark_agent_has_item (this, uri)))
		return;

	groups = g_bookmark_file_get_groups (priv->store, uri, NULL, NULL);

	for (i = 0; groups && groups [i]; ++i)
		if (g_str_has_prefix (groups [i], "rank-"))
			g_bookmark_file_remove_group (priv->store, uri, groups [i], NULL);

	g_strfreev (groups);

	group = g_strdup_printf ("rank-%d", rank);
	g_bookmark_file_add_group (priv->store, uri, group);
	g_free (group);
}

static void
save_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *dir;


	g_return_if_fail (priv->user_modifiable);

	priv->update_path (this);

	dir = g_path_get_dirname (priv->store_path);
	g_mkdir_with_parents (dir, 0700);
	g_free (dir);

	priv->save_store (this);
}

static gchar *
translate_uri (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *uri_new;


	if (! uri)
		return NULL;

	uri_new = priv->translate_uri (this, uri);

	if (libslab_strcmp (uri, uri_new))
		g_bookmark_file_move_item (priv->store, uri, uri_new, NULL);

	return uri_new;
}

static void
free_item (BookmarkItem *item)
{
	g_free (item->uri);
	g_free (item->title);
	g_free (item->mime_type);
	g_free (item->icon);
	g_free (item->app_name);
	g_free (item->app_exec);
	g_free (item);
}

static void
load_xbel_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar    **uris         = NULL;
	gchar    **uris_ordered = NULL;
	gsize      n_uris       = 0;
	gint       rank;
	gboolean   needs_update = FALSE;

	GError *error = NULL;

	gint i;


	if (! (priv->store_path && g_bookmark_file_load_from_file (priv->store, priv->store_path, & error))) {
		g_bookmark_file_free (priv->store);
		priv->store = g_bookmark_file_new ();

		libslab_handle_g_error (
			& error, "%s: couldn't load bookmark file [%s]\n",
			G_STRFUNC, priv->store_path);
	}

	uris = g_bookmark_file_get_uris (priv->store, & n_uris);
	uris_ordered = g_new0 (gchar *, n_uris + 1);
	uris_ordered [n_uris] = NULL;

	for (i = 0; uris && uris [i]; ++i) {
		rank = get_rank (this, uris [i]);

		if (rank < 0 || rank >= n_uris)
			rank = i;

		if (uris_ordered [rank])
			g_warning (
				"store corruption - multiple uris with same rank: [%s] rank = %d, [%s] [%s]",
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
			priv->items [i]->title     = g_bookmark_file_get_title     (priv->store, uris_ordered [i], NULL);
			priv->items [i]->mime_type = g_bookmark_file_get_mime_type (priv->store, uris_ordered [i], NULL);
			priv->items [i]->mtime     = g_bookmark_file_get_modified  (priv->store, uris_ordered [i], NULL);
			priv->items [i]->app_name  = NULL;
			priv->items [i]->app_exec  = NULL;

			g_bookmark_file_get_icon (priv->store, uris_ordered [i], & priv->items [i]->icon, NULL, NULL);
		}

		g_object_notify (G_OBJECT (this), BOOKMARK_AGENT_ITEMS_PROP);
	}

	g_strfreev (uris_ordered);
}

static void
update_user_spec_path (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gboolean  use_user_path;
	gchar    *path = NULL;

	BookmarkStoreStatus status;

	const gchar * const *dirs = NULL;
	gint                 i;


	use_user_path = priv->user_modifiable &&
		(priv->needs_sync || g_file_test (priv->user_store_path, G_FILE_TEST_EXISTS));

	if (use_user_path)
		path = g_strdup (priv->user_store_path);
	else {
		dirs = g_get_system_data_dirs ();

		for (i = 0; ! path && dirs && dirs [i]; ++i) {
			path = g_build_filename (dirs [i], PACKAGE, priv->store_filename, NULL);

			if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
				g_free (path);
				path = NULL;
			}
		}
	}

	if (use_user_path)
		status = BOOKMARK_STORE_USER;
	else if (path && priv->user_modifiable)
		status = BOOKMARK_STORE_DEFAULT;
	else if (path)
		status = BOOKMARK_STORE_DEFAULT_ONLY;
	else
		status = BOOKMARK_STORE_ABSENT;
	
	if (priv->status != status) {
		priv->status = status;
		g_object_notify (G_OBJECT (this), BOOKMARK_AGENT_STORE_STATUS_PROP);

		if (priv->user_store_monitor) {
			gnome_vfs_monitor_cancel (priv->user_store_monitor);
			priv->user_store_monitor = NULL;
		}

		if (priv->status == BOOKMARK_STORE_DEFAULT)
			gnome_vfs_monitor_add (
				& priv->store_monitor, priv->user_store_path,
				GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}

	if (libslab_strcmp (priv->store_path, path)) {
		g_free (priv->store_path);
		priv->store_path = path;

		if (priv->store_monitor)
			gnome_vfs_monitor_cancel (priv->store_monitor);

		if (priv->store_path)
			gnome_vfs_monitor_add (
				& priv->store_monitor, priv->store_path,
				GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}
	else
		g_free (path);
}

static void
save_xbel_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	GError *error = NULL;


	if (! g_bookmark_file_to_file (priv->store, priv->store_path, & error))
		libslab_handle_g_error (
			& error, "%s: couldn't save bookmark file [%s]\n", G_STRFUNC, priv->store_path);
}

static gchar *
translate_app_uri (BookmarkAgent *this, const gchar *uri)
{
	GnomeDesktopItem *ditem;
	gchar *uri_new = NULL;


	ditem = libslab_gnome_desktop_item_new_from_unknown_id (uri);

	if (ditem) {
		uri_new = g_strdup (gnome_desktop_item_get_location (ditem));
		gnome_desktop_item_unref (ditem);
	}

	return uri_new;
}

static gchar *
translate_doc_uri (BookmarkAgent *this, const gchar *uri)
{
	gchar *uri_new = NULL;
	gchar *path;
	gchar *dir;
	gchar *file;


	if (! (strcmp (uri, "BLANK_SPREADSHEET") && strcmp (uri, "BLANK_DOCUMENT"))) {
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

		g_free (dir);
		g_free (file);
		g_free (path);
	}

	return uri_new;
}

static gchar *
translate_dir_uri (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *uri_new = NULL;
	gchar *path;
	gchar *icon = NULL;


	if (! strcmp (uri, "HOME")) {
		uri_new = g_filename_to_uri (g_get_home_dir (), NULL, NULL);
		icon    = "gnome-fs-home";
	}
	else if (! strcmp (uri, "DOCUMENTS")) {
		path = g_build_filename (g_get_home_dir (), "Documents", NULL);
		uri_new = g_filename_to_uri (path, NULL, NULL);
		g_free (path);
	}
	else if (! strcmp (uri, "DESKTOP")) {
		path = g_build_filename (g_get_home_dir (), "Desktop", NULL);
		uri_new = g_filename_to_uri (path, NULL, NULL);
		icon = "gnome-fs-desktop";
		g_free (path);
	}
	else if (! strcmp (uri_new, "file:///"))
		icon = "drive-harddisk";
	else if (! strcmp (uri_new, "network:"))
		icon = "network-workgroup";
	else if (g_str_has_prefix (uri_new, "x-nautilus-search"))
		icon = "system-search";

	if (icon)
		g_bookmark_file_set_icon (priv->store, uri, uri_new, "image/png");

	return uri_new;
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


	user_modifiable = GPOINTER_TO_INT (libslab_get_gconf_value (priv->lockdown_key));

	if (priv->user_modifiable != user_modifiable) {
		priv->user_modifiable = user_modifiable;
		update_agent (this);
	}
}

#if 0
static gchar *
get_store_path (BookmarkAgent *this, gboolean writeable)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *user_path = NULL;
	gchar *sys_path  = NULL;
	gchar *path      = NULL;

	gchar *user_path_copy;

	BookmarkStoreStatus status;

	const gchar * const *dirs = NULL;
	gint                 i;


	if (TYPE_IS_USER_SPEC (priv->type)) {
		priv->user_modifiable = GPOINTER_TO_INT (
			libslab_get_gconf_value (lockdown_keys [priv->type]));

		user_path = g_build_filename (
			g_get_user_data_dir (), PACKAGE, store_file_names [priv->type], NULL);
		user_path_copy = g_strdup (user_path);

		if (! priv->user_modifiable || ! (writeable || g_file_test (user_path, G_FILE_TEST_EXISTS))) {
			g_free (user_path);
			user_path = NULL;
		}

		if (! (user_path || writeable)) {
			dirs = g_get_system_data_dirs ();

			for (i = 0; ! sys_path && dirs && dirs [i]; ++i) {
				sys_path = g_build_filename (
					dirs [i], PACKAGE, store_file_names [priv->type], NULL);

				if (! g_file_test (sys_path, G_FILE_TEST_EXISTS)) {
					g_free (sys_path);
					sys_path = NULL;
				}
			}
		}

		if (user_path) {
			status = BOOKMARK_STORE_USER;
			path   = user_path;
		}
		else if (sys_path) {
			if (priv->user_modifiable)
				status = BOOKMARK_STORE_DEFAULT;
			else
				status = BOOKMARK_STORE_DEFAULT_ONLY;

			path = sys_path;
		}
		else
			status = BOOKMARK_STORE_ABSENT;
	}
	else {
		path = g_build_filename (g_get_home_dir (), ".recently-used", NULL);

		if (g_file_test (sys_path, G_FILE_TEST_EXISTS))
			status = BOOKMARK_STORE_USER;
		else
			status = BOOKMARK_STORE_ABSENT;
	}
	
	if (priv->status != status) {
		priv->status = status;
		g_object_notify (G_OBJECT (this), BOOKMARK_AGENT_STORE_STATUS_PROP);

		if (priv->user_store_monitor) {
			gnome_vfs_monitor_cancel (priv->user_store_monitor);
			priv->user_store_monitor = NULL;
		}

		if (priv->status == BOOKMARK_STORE_DEFAULT)
			gnome_vfs_monitor_add (
				& priv->store_monitor, user_path_copy,
				GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}

	g_free (user_path_copy);

	return path;
}
#endif

#if 0
#endif

#if 0
static void
sync_gtk_bookmarks_to_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar **uris;
	gchar **groups;

	gchar *gtk_bookmarks_path;

	gchar  *buf;
	gchar **folders = NULL;

	gint i, j;


	if (priv->type != BOOKMARK_STORE_USER_DIRS)
		return;

	uris = g_bookmark_file_get_uris (priv->store, NULL);

	for (i = 0; uris && uris [i]; ++i) {
		groups = g_bookmark_file_get_groups (priv->store, uris [i], NULL, NULL);

		for (j = 0; groups && groups [j]; ++j) {
			if (! strcmp (groups [j], "gtk-bookmarks")) {
				g_bookmark_file_remove_item (priv->store, uris [i], NULL);

				break;
			}
		}

		g_strfreev (groups);
	}

	g_strfreev (uris);

	gtk_bookmarks_path = g_build_filename (g_get_home_dir (), GTK_BOOKMARKS_FILE, NULL);
	g_file_get_contents (gtk_bookmarks_path, & buf, NULL, NULL);

	if (buf) {
		folders = g_strsplit (buf, "\n", -1);
		g_free (buf);
	}

	for (i = 0; folders && folders [i]; ++i) {
		if (strlen (folders [i]) > 0) {
			g_bookmark_file_set_mime_type (priv->store, folders [i], "inode/directory");
			g_bookmark_file_add_application (
				priv->store, folders [i], "nautilus", "nautilus --browser %u");
			g_bookmark_file_add_group (priv->store, folders [i], "gtk-bookmarks");
		}
	}

	g_strfreev (folders);
}
#endif
