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

#define USER_APPS_STORE_FILE_NAME "applications.xbel"
#define USER_DOCS_STORE_FILE_NAME "documents.xbel"

#define TYPE_IS_USER_SPEC(type) ((type) == BOOKMARK_STORE_USER_APPS || (type) == BOOKMARK_STORE_USER_DOCS)

typedef struct {
	BookmarkStoreType      type;

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

static BookmarkAgent *instances        [BOOKMARK_STORE_N_TYPES];
static gchar         *lockdown_keys    [BOOKMARK_STORE_N_TYPES];
static gchar         *store_file_names [BOOKMARK_STORE_N_TYPES];

static BookmarkAgentClass *bookmark_agent_parent_class = NULL;

static void bookmark_agent_base_init  (BookmarkAgentClass *);
static void bookmark_agent_class_init (BookmarkAgentClass *);
static void bookmark_agent_init       (BookmarkAgent      *);

static void get_property     (GObject *, guint, GValue *, GParamSpec *);
static void set_property     (GObject *, guint, const GValue *, GParamSpec *);
static void finalize         (GObject *);

static void   update_agent   (BookmarkAgent *);
static void   save_store     (BookmarkAgent *);
static gchar *get_store_path (BookmarkAgent *, gboolean);
static gchar *translate_uri  (BookmarkAgent *, const gchar *);
static void   free_item      (BookmarkItem *);
static gint   get_rank       (BookmarkAgent *, const gchar *);
static void   set_rank       (BookmarkAgent *, const gchar *, gint);

static void store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                              GnomeVFSMonitorEventType, gpointer);
static void gconf_notify_cb  (GConfClient *, guint, GConfEntry *, gpointer);

GType
bookmark_agent_get_type ()
{
	static GType g_define_type_id = 0;

	if (G_UNLIKELY (g_define_type_id == 0)) {
		static const GTypeInfo info = {
			sizeof (BookmarkAgentClass),
			(GBaseInitFunc) bookmark_agent_base_init,
			NULL,
			(GClassInitFunc) bookmark_agent_class_init,
			NULL, NULL,
			sizeof (BookmarkAgent), 0,
			(GInstanceInitFunc) bookmark_agent_init,
			NULL
		};

		g_define_type_id = g_type_register_static (
			G_TYPE_OBJECT, "BookmarkAgent", & info, 0);
	}

	return g_define_type_id;
}

BookmarkAgent *
bookmark_agent_get_instance (BookmarkStoreType type)
{
	BookmarkAgentPrivate *priv;

	if (type < 0 || type >= BOOKMARK_STORE_N_TYPES)
		return NULL;

	if (! instances [type]) {
		instances [type] = g_object_new (BOOKMARK_AGENT_TYPE, NULL);
		priv             = PRIVATE (instances [type]);

		priv->type          = type;
		priv->store         = g_bookmark_file_new ();
		priv->gconf_monitor = libslab_gconf_notify_add (
			lockdown_keys [type], gconf_notify_cb, instances [type]);

		update_agent (instances [type]);
	}
	else
		g_object_ref (G_OBJECT (instances [type]));

	return instances [type];
}

gboolean
bookmark_agent_has_item (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	if (! uri)
		return FALSE;

	switch (priv->type) {
		case BOOKMARK_STORE_USER_APPS:
		case BOOKMARK_STORE_USER_DOCS:
			return g_bookmark_file_has_item (PRIVATE (this)->store, uri);

		default:
			return TRUE;
	}
}

void
bookmark_agent_add_item (BookmarkAgent *this, const BookmarkItem *item)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	if (! (item->uri && item->mime_type && TYPE_IS_USER_SPEC (priv->type)))
		return;

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


	if (! bookmark_agent_has_item (this, uri))
		return;

	rank = get_rank (this, uri);

	switch (priv->type) {
		case BOOKMARK_STORE_USER_APPS:
		case BOOKMARK_STORE_USER_DOCS:
			g_bookmark_file_remove_item (priv->store, uri, NULL);
			break;

		default: 
			 break;
	}                
                         
	if (rank >= 0 &&  TYPE_IS_USER_SPEC (priv->type)) {
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


	if (! TYPE_IS_USER_SPEC (priv->type))
		return;

	for (i = 0; uris && uris [i]; ++i)
		set_rank (this, uris [i], i);

	save_store (this);
}

static void
bookmark_agent_base_init (BookmarkAgentClass *this_class)
{
	gint i;

	for (i = 0; i < BOOKMARK_STORE_N_TYPES; ++i)
		instances [i] = NULL;

	lockdown_keys [BOOKMARK_STORE_USER_APPS] = MODIFIABLE_APPS_GCONF_KEY;
	lockdown_keys [BOOKMARK_STORE_USER_DOCS] = MODIFIABLE_DOCS_GCONF_KEY;

	store_file_names [BOOKMARK_STORE_USER_APPS] = USER_APPS_STORE_FILE_NAME;
	store_file_names [BOOKMARK_STORE_USER_DOCS] = USER_DOCS_STORE_FILE_NAME;
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

	signals [UPDATE_SIGNAL] = g_signal_new (
		BOOKMARK_AGENT_UPDATE_SIGNAL,
		G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (BookmarkAgentClass, update),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	g_type_class_add_private (this_class, sizeof (BookmarkAgentPrivate));

	bookmark_agent_parent_class = g_type_class_peek_parent (this_class);
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

	if (TYPE_IS_USER_SPEC (priv->type))
		g_bookmark_file_free (priv->store);
	else
		/* FIXME */ ;

	G_OBJECT_CLASS (bookmark_agent_parent_class)->finalize (g_obj);
}

static void
update_agent (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *store_path;

	gchar    **uris         = NULL;
	gchar    **uris_ordered = NULL;
	gsize      n_uris       = 0;
	gint       rank;
	gboolean   needs_update = FALSE;

	gchar  *title     = NULL;
	gchar  *mime_type = NULL;
	time_t  mtime     = 0;
	gchar  *app_name  = NULL;
	gchar  *app_exec  = NULL;

	GError *error = NULL;

	gint i;


	store_path = get_store_path (this, FALSE);

	if (libslab_strcmp (priv->store_path, store_path)) {
		g_free (priv->store_path);
		priv->store_path = g_strdup (store_path);

		if (priv->store_monitor)
			gnome_vfs_monitor_cancel (priv->store_monitor);

		if (priv->store_path)
			gnome_vfs_monitor_add (
				& priv->store_monitor, priv->store_path,
				GNOME_VFS_MONITOR_FILE, store_monitor_cb, this);
	}

	switch (priv->type) {
		case BOOKMARK_STORE_USER_APPS:
		case BOOKMARK_STORE_USER_DOCS:
			if (priv->store_path)
				g_bookmark_file_load_from_file (priv->store, priv->store_path, & error);
			else {
				g_bookmark_file_free (priv->store);
				priv->store = g_bookmark_file_new ();
			}

			if (error)
				libslab_handle_g_error (
					& error, "%s: couldn't load bookmark file [%s]\n",
					G_STRFUNC, priv->store_path);

			uris = g_bookmark_file_get_uris (priv->store, & n_uris);
			break;

		default:
			break;
	}

	uris_ordered = g_new0 (gchar *, n_uris + 1);

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
			switch (priv->type) {
				case BOOKMARK_STORE_USER_APPS:
				case BOOKMARK_STORE_USER_DOCS:
					title     = g_bookmark_file_get_title     (priv->store, uris_ordered [i], NULL);
					mime_type = g_bookmark_file_get_mime_type (priv->store, uris_ordered [i], NULL);
					mtime     = g_bookmark_file_get_modified  (priv->store, uris_ordered [i], NULL);
					app_name  = NULL;
					app_exec  = NULL;

				default:
					break;
			}

			priv->items [i]            = g_new0 (BookmarkItem, 1);
			priv->items [i]->uri       = g_strdup (uris_ordered [i]);
			priv->items [i]->title     = title;
			priv->items [i]->mime_type = mime_type;
			priv->items [i]->mtime     = mtime;
			priv->items [i]->app_name  = app_name;
			priv->items [i]->app_exec  = app_exec;
		}

		g_signal_emit (this, signals [UPDATE_SIGNAL], 0);
	}

	g_strfreev (uris_ordered);
}

static void
save_store (BookmarkAgent *this)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *store_path;
	gchar *dir;

	GError *error = NULL;


	if (! TYPE_IS_USER_SPEC (priv->type))
		return;

	store_path = get_store_path (this, TRUE);

	dir = g_path_get_dirname (store_path);
	g_mkdir_with_parents (dir, 0700);
	g_free (dir);

	if (! g_bookmark_file_to_file (priv->store, store_path, & error))
		libslab_handle_g_error (
			& error, "%s: couldn't save bookmark file [%s]\n", G_STRFUNC, store_path);

	g_free (store_path);
}

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

static gchar *
translate_uri (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar *uri_new = NULL;
	gchar *path;
	gchar *dir;
	gchar *file;

	GnomeDesktopItem *ditem;


	if (! uri)
		return NULL;

	switch (priv->type) {
		case BOOKMARK_STORE_USER_APPS:
			ditem = libslab_gnome_desktop_item_new_from_unknown_id (uri);
			uri_new = g_strdup (gnome_desktop_item_get_location (ditem));
			gnome_desktop_item_unref (ditem);

			break;

		case BOOKMARK_STORE_USER_DOCS:
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

			break;

		default:
			break;
	}

	if (! uri_new)
		uri_new = g_strdup (uri);

	if (strcmp (uri, uri_new)) {
		if (TYPE_IS_USER_SPEC (priv->type))
			g_bookmark_file_move_item (priv->store, uri, uri_new, NULL);
		else
			/* FIXME */ ;
	}

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

static gint
get_rank (BookmarkAgent *this, const gchar *uri)
{
	BookmarkAgentPrivate *priv = PRIVATE (this);

	gchar **groups;
	gint    rank;

	gint i;


	if (! TYPE_IS_USER_SPEC (priv->type))
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


	if (! TYPE_IS_USER_SPEC (priv->type) || ! g_bookmark_file_has_item (priv->store, uri))
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


	user_modifiable = GPOINTER_TO_INT (libslab_get_gconf_value (lockdown_keys [priv->type]));

	if (priv->user_modifiable != user_modifiable) {
		priv->user_modifiable = user_modifiable;
		update_agent (this);
	}
}
