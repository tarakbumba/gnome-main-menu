#include "file-tile-model.h"

#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "libslab-utils.h"
#include "bookmark-agent.h"

typedef struct {
	gchar                   *uri;
	gchar                   *mime_type;

	BookmarkAgent           *user_agent;
	BookmarkAgent           *recent_agent;

	TileAttribute           *file_name_attr;
	TileAttribute           *icon_id_attr;
	TileAttribute           *mtime_attr;
	TileAttribute           *app_attr;
	TileAttribute           *is_local_attr;
	TileAttribute           *is_in_store_attr;
	TileAttribute           *store_status_attr;
	TileAttribute           *can_delete_attr;

	GnomeVFSAsyncHandle     *handle;
	GnomeVFSMimeApplication *default_app;

	guint                    enable_delete_monitor_id;
} FileTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FILE_TILE_MODEL_TYPE, FileTileModelPrivate))

#define OPEN_IN_FILE_BROWSER_CMD_KEY "/desktop/gnome/applications/main-menu/file-area/file_mgr_open_cmd"
#define SEND_TO_CMD_KEY              "/desktop/gnome/applications/main-menu/file-area/file_send_to_cmd"
#define ENABLE_DELETE_KEY_DIR        "/apps/nautilus/preferences"
#define ENABLE_DELETE_KEY            ENABLE_DELETE_KEY_DIR "/enable_delete"

#define DEFAULT_ICON_ID  "gnome-fs-regular"
#define MAX_DESC_STR_LEN 1024

static void this_class_init (FileTileModelClass *);
static void this_init       (FileTileModel *);

static void finalize (GObject *);

static void     update_model     (FileTileModel *);
static void     update_mime_type (FileTileModel *, const gchar *);
static gboolean file_is_local    (FileTileModel *);

static void thumbnail_factory_destroy_cb (gpointer, GObject *);
static void volume_monitor_destroy_cb    (gpointer, GObject *);
static void uri_attr_notify_cb           (GObject *, GParamSpec *, gpointer);
static void user_store_items_notify_cb   (GObject *, GParamSpec *, gpointer);
static void user_store_status_notify_cb  (GObject *, GParamSpec *, gpointer);
static void enable_delete_notify_cb      (GConfClient *, guint, GConfEntry *, gpointer);
static void file_info_cb                 (GnomeVFSAsyncHandle *, GList *, gpointer);

static GnomeThumbnailFactory *thumbnail_factory = NULL;
static GnomeVFSVolumeMonitor *volume_monitor    = NULL;

static GObjectClass *this_parent_class = NULL;

GType
file_tile_model_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_MODEL_TYPE, "FileTileModel",
			sizeof (FileTileModelClass), (GClassInitFunc) this_class_init,
			sizeof (FileTileModel), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

FileTileModel *
file_tile_model_new (const gchar *uri)
{
	FileTileModel        *this;
	FileTileModelPrivate *priv;
	
	
	this = g_object_new (FILE_TILE_MODEL_TYPE, NULL);
	tile_attribute_set_string (tile_model_get_uri_attr (TILE_MODEL (this)), uri);

	priv = PRIVATE (this);

	priv->uri = g_strdup (uri);

	priv->user_agent   = bookmark_agent_get_instance (BOOKMARK_STORE_USER_DOCS);
	priv->recent_agent = bookmark_agent_get_instance (BOOKMARK_STORE_RECENT_DOCS);

	priv->file_name_attr    = tile_attribute_new (G_TYPE_STRING);
	priv->icon_id_attr      = tile_attribute_new (G_TYPE_STRING);
	priv->mtime_attr        = tile_attribute_new (G_TYPE_LONG);
	priv->app_attr          = tile_attribute_new (G_TYPE_POINTER);
	priv->is_local_attr     = tile_attribute_new (G_TYPE_BOOLEAN);
	priv->is_in_store_attr  = tile_attribute_new (G_TYPE_BOOLEAN);
	priv->store_status_attr = tile_attribute_new (G_TYPE_INT);
	priv->can_delete_attr   = tile_attribute_new (G_TYPE_BOOLEAN);

	update_model (this);

	tile_attribute_set_int (
		priv->store_status_attr, bookmark_agent_get_status (priv->user_agent));
	tile_attribute_set_boolean (
		priv->can_delete_attr,
		GPOINTER_TO_INT (libslab_get_gconf_value (ENABLE_DELETE_KEY)));

	priv->enable_delete_monitor_id = libslab_gconf_notify_add (
		ENABLE_DELETE_KEY, enable_delete_notify_cb, this);

	g_signal_connect (
		tile_model_get_uri_attr (TILE_MODEL (this)), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (uri_attr_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->user_agent), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
		G_CALLBACK (user_store_items_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->user_agent), "notify::" BOOKMARK_AGENT_STORE_STATUS_PROP,
		G_CALLBACK (user_store_status_notify_cb), this);

	return this;
}

TileAttribute *
file_tile_model_get_file_name_attr (FileTileModel *this)
{
	return PRIVATE (this)->file_name_attr;
}

TileAttribute *
file_tile_model_get_icon_id_attr (FileTileModel *this)
{
	return PRIVATE (this)->icon_id_attr;
}

TileAttribute *
file_tile_model_get_mtime_attr (FileTileModel *this)
{
	return PRIVATE (this)->mtime_attr;
}

TileAttribute *
file_tile_model_get_app_attr (FileTileModel *this)
{
	return PRIVATE (this)->app_attr;
}

TileAttribute *
file_tile_model_get_is_local_attr (FileTileModel *this)
{
	return PRIVATE (this)->is_local_attr;
}

TileAttribute *
file_tile_model_get_is_in_store_attr (FileTileModel *this)
{
	return PRIVATE (this)->is_in_store_attr;
}

TileAttribute *
file_tile_model_get_store_status_attr (FileTileModel *this)
{
	return PRIVATE (this)->store_status_attr;
}

TileAttribute *
file_tile_model_get_can_delete_attr (FileTileModel *this)
{
	return PRIVATE (this)->can_delete_attr;
}

void
file_tile_model_open (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GList *uris = NULL;
	GnomeVFSResult retval;


	if (priv->default_app) {
		uris = g_list_append (uris, priv->uri);

		retval = gnome_vfs_mime_application_launch (priv->default_app, uris);

		if (retval != GNOME_VFS_OK)
			g_warning
				("error: could not open [%s] with \"%s\", GnomeVFSResult = %s\n",
				priv->uri,
				gnome_vfs_mime_application_get_name (priv->default_app),
				gnome_vfs_result_to_string (retval));

		g_list_free (uris);
	}
}

void
file_tile_model_open_in_file_browser (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	gchar *filename;
	gchar *dirname;
	gchar *uri;

	gchar *cmd_template;
	gchar *cmd;


	filename = g_filename_from_uri (priv->uri, NULL, NULL);
	dirname  = g_path_get_dirname (filename);
	uri      = g_filename_to_uri (dirname, NULL, NULL);

	if (! uri)
		g_warning ("error getting dirname for [%s]\n", priv->uri);
	else {
		cmd_template = (gchar *) libslab_get_gconf_value (OPEN_IN_FILE_BROWSER_CMD_KEY);
		cmd = libslab_string_replace_once (cmd_template, "FILE_URI", uri);

		libslab_spawn_command (cmd);

		g_free (cmd);
		g_free (cmd_template);
	}

	g_free (filename);
	g_free (dirname);
	g_free (uri);
}

void
file_tile_model_rename (FileTileModel *this, const gchar *name)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GnomeVFSURI *src_uri;
	GnomeVFSURI *dst_uri;

	gchar *dirname;
	gchar *dst_path;
	gchar *dst_uri_str;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (priv->uri);
	dirname = gnome_vfs_uri_extract_dirname (src_uri);

	dst_path = g_build_filename (dirname, name, NULL);
	dst_uri  = gnome_vfs_uri_new (dst_path);

	if (! gnome_vfs_uri_equal (src_uri, dst_uri)) {
		retval = gnome_vfs_xfer_uri (
			src_uri, dst_uri,
			GNOME_VFS_XFER_REMOVESOURCE,
			GNOME_VFS_XFER_ERROR_MODE_ABORT,
			GNOME_VFS_XFER_OVERWRITE_MODE_SKIP,
			NULL, NULL);

		if (retval == GNOME_VFS_OK) {
			dst_uri_str = gnome_vfs_uri_to_string (dst_uri, GNOME_VFS_URI_HIDE_NONE);

			if (bookmark_agent_has_item (priv->recent_agent, priv->uri))
				bookmark_agent_move_item (priv->recent_agent, priv->uri, dst_uri_str);

			tile_attribute_set_string (
				tile_model_get_uri_attr (TILE_MODEL (this)), dst_uri_str);

			g_free (dst_uri_str);
		}
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dst_uri);

	g_free (dirname);
	g_free (dst_path);
}

void
file_tile_model_send_to (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	gchar *cmd_template;
	gchar *cmd;

	gchar *filename;
	gchar *dirname;
	gchar *basename;


	filename = g_filename_from_uri (priv->uri, NULL, NULL);
	dirname  = g_path_get_dirname (filename);
	basename = g_path_get_basename (filename);

	cmd_template = (gchar *) libslab_get_gconf_value (SEND_TO_CMD_KEY);
	cmd = libslab_string_replace_once (cmd_template, "DIRNAME", dirname);
	g_free (cmd_template);

	cmd_template = cmd;
	cmd = libslab_string_replace_once (cmd_template, "BASENAME", basename);
	g_free (cmd_template);

	libslab_spawn_command (cmd);

	g_free (cmd);
	g_free (filename);
	g_free (dirname);
	g_free (basename);
}

void
file_tile_model_user_store_toggle (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	BookmarkItem            *item;
	GnomeVFSMimeApplication *app;


	if (bookmark_agent_has_item (priv->user_agent, priv->uri))
		bookmark_agent_remove_item (priv->user_agent, priv->uri);
	else {
		app = (GnomeVFSMimeApplication *)
			g_value_get_pointer (tile_attribute_get_value (priv->app_attr));

		item = g_new0 (BookmarkItem, 1);
		item->uri       = priv->uri;
		item->mime_type = priv->mime_type;
		item->mtime     = g_value_get_long (tile_attribute_get_value (priv->mtime_attr));
		item->app_name  = (gchar *) gnome_vfs_mime_application_get_name (app);
		item->app_exec  = (gchar *) gnome_vfs_mime_application_get_exec (app);

		bookmark_agent_add_item (priv->user_agent, item);
		g_free (item);
	}
}

void
file_tile_model_trash (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GnomeVFSURI *src_uri;
	GnomeVFSURI *trash_uri;

	gchar *file_name;
	gchar *trash_uri_str;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (priv->uri);

	gnome_vfs_find_directory (
		src_uri, GNOME_VFS_DIRECTORY_KIND_TRASH, & trash_uri, FALSE, FALSE, 0777);

	if (! trash_uri) {
		g_warning ("unable to find trash location\n");

		return;
	}

	file_name = gnome_vfs_uri_extract_short_name (src_uri);

	if (! file_name) {
		g_warning ("unable to extract short name from [%s]\n", priv->uri);

		return;
	}

	trash_uri = gnome_vfs_uri_append_file_name (trash_uri, file_name);

	retval = gnome_vfs_xfer_uri (
		src_uri, trash_uri,
		GNOME_VFS_XFER_REMOVESOURCE, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, NULL, NULL);

	if (retval == GNOME_VFS_OK) {
		bookmark_agent_remove_item (priv->user_agent, priv->uri);
		bookmark_agent_remove_item (priv->recent_agent, priv->uri);
	}
	else {
		trash_uri_str = gnome_vfs_uri_to_string (trash_uri, GNOME_VFS_URI_HIDE_NONE);

		g_warning ("unable to move [%s] to the trash [%s]\n", priv->uri, trash_uri_str);

		g_free (trash_uri_str);
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (trash_uri);

	g_free (file_name);
}

void
file_tile_model_delete (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GnomeVFSURI *src_uri;
	GList *list = NULL;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (priv->uri);

	list = g_list_append (list, src_uri);

	retval = gnome_vfs_xfer_delete_list (
		list, GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);

	if (retval == GNOME_VFS_OK) {
		bookmark_agent_remove_item (priv->user_agent, priv->uri);
		bookmark_agent_remove_item (priv->recent_agent, priv->uri);
	}
	else
		g_warning ("unable to delete [%s]\n", priv->uri);

	gnome_vfs_uri_unref (src_uri);
	g_list_free (list);
}

static void
this_class_init (FileTileModelClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (FileTileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GConfClient *client;


	priv->uri                      = NULL;
	priv->mime_type                = NULL;

	priv->user_agent               = NULL;
	priv->recent_agent             = NULL;

	priv->file_name_attr           = NULL;
	priv->icon_id_attr             = NULL;
	priv->mtime_attr               = NULL;
	priv->app_attr                 = NULL;
	priv->is_local_attr            = NULL;
	priv->is_in_store_attr         = NULL;
	priv->store_status_attr        = NULL;
	priv->can_delete_attr          = NULL;

	priv->handle                   = NULL;
	priv->default_app              = NULL;

	priv->enable_delete_monitor_id = 0;

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, ENABLE_DELETE_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	g_object_unref (client);

	if (! thumbnail_factory) {
		thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
		g_object_weak_ref (G_OBJECT (thumbnail_factory), thumbnail_factory_destroy_cb, NULL);
	}
	else
		g_object_ref (G_OBJECT (thumbnail_factory));

	if (! volume_monitor) {
		volume_monitor = gnome_vfs_get_volume_monitor ();
		g_object_weak_ref (G_OBJECT (volume_monitor), volume_monitor_destroy_cb, NULL);
	}
	else
		gnome_vfs_volume_monitor_ref (volume_monitor);
}

static void
finalize (GObject *g_obj)
{
	FileTileModelPrivate *priv = PRIVATE (g_obj);

	GConfClient *client;


	gnome_vfs_async_cancel (priv->handle);

	g_free (priv->uri);
	g_free (priv->mime_type);

	g_object_unref (priv->user_agent);
	g_object_unref (priv->recent_agent);

	g_object_unref (priv->file_name_attr);
	g_object_unref (priv->icon_id_attr);
	g_object_unref (priv->mtime_attr);
	g_object_unref (priv->app_attr);
	g_object_unref (priv->is_local_attr);
	g_object_unref (priv->is_in_store_attr);
	g_object_unref (priv->store_status_attr);
	g_object_unref (priv->can_delete_attr);

	g_object_unref (G_OBJECT (thumbnail_factory));
	gnome_vfs_volume_monitor_unref (volume_monitor);

	libslab_gconf_notify_remove (priv->enable_delete_monitor_id);

	client = gconf_client_get_default ();
	gconf_client_remove_dir (client, ENABLE_DELETE_KEY_DIR, NULL);
	g_object_unref (client);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
update_model (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	gchar *basename;
	gchar *file_name;

	GnomeVFSURI *uri;
	GList       *uri_list = NULL;


	basename = g_path_get_basename (priv->uri);

	file_name = gnome_vfs_unescape_string (basename, NULL);
	tile_attribute_set_string (priv->file_name_attr, file_name);
	g_free (file_name);

	update_mime_type (this, gnome_vfs_get_mime_type_for_name (basename));

	g_free (basename);

	tile_attribute_set_string (priv->icon_id_attr, DEFAULT_ICON_ID);
	tile_attribute_set_boolean (priv->is_local_attr, file_is_local (this));

	uri      = gnome_vfs_uri_new (priv->uri);
	uri_list = g_list_append (uri_list, uri);

	gnome_vfs_async_get_file_info (
		& priv->handle, uri_list, GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
		GNOME_VFS_PRIORITY_DEFAULT, file_info_cb, this);

	tile_attribute_set_boolean (
		priv->is_in_store_attr,
		bookmark_agent_has_item (priv->user_agent, priv->uri));

	gnome_vfs_uri_unref (uri);
	g_list_free (uri_list);
}

static void
update_mime_type (FileTileModel *this, const gchar *mime_type)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	if (! libslab_strcmp (priv->mime_type, mime_type))
		return;

	g_free (priv->mime_type);
	priv->mime_type = g_strdup (mime_type);

	gnome_vfs_mime_application_free (priv->default_app);
	priv->default_app = gnome_vfs_mime_get_default_application (priv->mime_type);

	tile_attribute_set_pointer (priv->app_attr, priv->default_app);
}

static gboolean
file_is_local (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	GList          *mounts;
	GnomeVFSVolume *vol;
	gchar          *mount_point;
	GnomeVFSURI    *gvfs_uri;
	gboolean        is_local = TRUE;

	GList *node;


	if (! g_str_has_prefix (priv->uri, "file://"))
		return FALSE;

	mounts = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);

	for (node = mounts; is_local && node; node = node->next) {
		vol = (GnomeVFSVolume *) node->data;

		if (gnome_vfs_volume_get_device_type (vol) == GNOME_VFS_DEVICE_TYPE_NFS) {
			mount_point = gnome_vfs_volume_get_activation_uri (vol);
			is_local = ! g_str_has_prefix (priv->uri, mount_point);
			g_free (mount_point);
		}
	}

	if (is_local) {
		gvfs_uri = gnome_vfs_uri_new (priv->uri);
		is_local = gnome_vfs_uri_is_local (gvfs_uri);
		gnome_vfs_uri_unref (gvfs_uri);
	}

	g_list_foreach (mounts, (GFunc) gnome_vfs_volume_unref, NULL);
	g_list_free (mounts);

	return is_local;
}

static void
thumbnail_factory_destroy_cb (gpointer data, GObject *g_obj)
{
	thumbnail_factory = NULL;
}

static void
volume_monitor_destroy_cb (gpointer data, GObject *g_obj)
{
	volume_monitor = NULL;
}

static void
uri_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	FileTileModel        *this = FILE_TILE_MODEL (data);
	FileTileModelPrivate *priv = PRIVATE (this);

	const gchar *uri;


	uri = tile_model_get_uri (TILE_MODEL (this));

	if (libslab_strcmp (uri, priv->uri)) {
		g_free (priv->uri);
		priv->uri = g_strdup (uri);
		update_model (this);
	}
}

static void
user_store_items_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	FileTileModelPrivate *priv = PRIVATE (data);

	tile_attribute_set_boolean (
		priv->is_in_store_attr,
		bookmark_agent_has_item (priv->user_agent, priv->uri));
}

static void
user_store_status_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	FileTileModelPrivate *priv = PRIVATE (data);

	tile_attribute_set_int (
		priv->store_status_attr, bookmark_agent_get_status (priv->user_agent));
}

static void
enable_delete_notify_cb (GConfClient *client, guint conn_id,
                         GConfEntry *entry, gpointer data)
{
	tile_attribute_set_boolean (
		PRIVATE (data)->can_delete_attr,
		GPOINTER_TO_INT (libslab_get_gconf_value (ENABLE_DELETE_KEY)));
}

static void
file_info_cb (GnomeVFSAsyncHandle *handle, GList *results, gpointer data)
{
	GObject              *this = G_OBJECT (data);
	FileTileModelPrivate *priv = PRIVATE  (this);

	GnomeVFSGetFileInfoResult *result;

	gchar *icon_id;


	if (! results)
		return;

	result = (GnomeVFSGetFileInfoResult *) results->data;

	if (! (result && result->result == GNOME_VFS_OK))
		return;

	tile_attribute_set_string (priv->file_name_attr, result->file_info->name);

	if (result->file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		update_mime_type (FILE_TILE_MODEL (this), result->file_info->mime_type);

	if (result->file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
		tile_attribute_set_long (priv->mtime_attr, result->file_info->mtime);

	icon_id = gnome_icon_lookup (
		gtk_icon_theme_get_default (), thumbnail_factory, priv->uri, NULL,
		result->file_info, priv->mime_type, 0, NULL);
	tile_attribute_set_string (priv->icon_id_attr, icon_id);
	g_free (icon_id);
}
