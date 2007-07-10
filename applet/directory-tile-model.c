#include "directory-tile-model.h"

#include <glib/gi18n.h>
#include <string.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "libslab-utils.h"

typedef struct {
	gchar         *uri;

	TileAttribute *icon_id_attr;
	TileAttribute *name_attr;
	TileAttribute *is_local_attr;
	TileAttribute *can_delete_attr;

	guint          enable_delete_monitor_id;
} DirectoryTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DIRECTORY_TILE_MODEL_TYPE, DirectoryTileModelPrivate))

#define OPEN_IN_FILE_BROWSER_CMD_KEY "/desktop/gnome/applications/main-menu/file-area/file_mgr_open_cmd"
#define SEND_TO_CMD_KEY              "/desktop/gnome/applications/main-menu/file-area/file_send_to_cmd"
#define ENABLE_DELETE_KEY_DIR        "/apps/nautilus/preferences"
#define ENABLE_DELETE_KEY            ENABLE_DELETE_KEY_DIR "/enable_delete"

#define DEFAULT_ICON_ID "gnome-fs-directory"

static void this_class_init (DirectoryTileModelClass *);
static void this_init       (DirectoryTileModel *);

static void finalize (GObject *);

static void     update_model     (DirectoryTileModel *);
static gboolean is_home_dir      (DirectoryTileModel *);
static gboolean is_documents_dir (DirectoryTileModel *);
static gboolean is_desktop_dir   (DirectoryTileModel *);
static gboolean dir_is_local     (DirectoryTileModel *);

static void volume_monitor_destroy_cb (gpointer, GObject *);
static void uri_attr_notify_cb        (GObject *, GParamSpec *, gpointer);
static void enable_delete_notify_cb   (GConfClient *, guint, GConfEntry *, gpointer);

static GnomeVFSVolumeMonitor *volume_monitor = NULL;

static GObjectClass *this_parent_class = NULL;

GType
directory_tile_model_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_MODEL_TYPE, "DirectoryTileModel",
			sizeof (DirectoryTileModelClass), (GClassInitFunc) this_class_init,
			sizeof (DirectoryTileModel), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

DirectoryTileModel *
directory_tile_model_new (const gchar *uri)
{
	DirectoryTileModel        *this;
	DirectoryTileModelPrivate *priv;
	
	
	this = g_object_new (DIRECTORY_TILE_MODEL_TYPE, NULL);
	tile_attribute_set_string (tile_model_get_uri_attr (TILE_MODEL (this)), uri);

	priv = PRIVATE (this);

	priv->uri = g_strdup (uri);

	priv->icon_id_attr    = tile_attribute_new (G_TYPE_STRING);
	priv->name_attr       = tile_attribute_new (G_TYPE_STRING);
	priv->is_local_attr   = tile_attribute_new (G_TYPE_BOOLEAN);
	priv->can_delete_attr = tile_attribute_new (G_TYPE_BOOLEAN);

	update_model (this);

	tile_attribute_set_boolean (
		priv->can_delete_attr,
		GPOINTER_TO_INT (libslab_get_gconf_value (ENABLE_DELETE_KEY)));

	priv->enable_delete_monitor_id = libslab_gconf_notify_add (
		ENABLE_DELETE_KEY, enable_delete_notify_cb, this);

	g_signal_connect (
		tile_model_get_uri_attr (TILE_MODEL (this)), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (uri_attr_notify_cb), this);

	return this;
}

TileAttribute *
directory_tile_model_get_icon_id_attr (DirectoryTileModel *this)
{
	return PRIVATE (this)->icon_id_attr;
}

TileAttribute *
directory_tile_model_get_name_attr (DirectoryTileModel *this)
{
	return PRIVATE (this)->name_attr;
}

TileAttribute *
directory_tile_model_get_is_local_attr (DirectoryTileModel *this)
{
	return PRIVATE (this)->is_local_attr;
}

TileAttribute *
directory_tile_model_get_can_delete_attr (DirectoryTileModel *this)
{
	return PRIVATE (this)->can_delete_attr;
}

void
directory_tile_model_open (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

	gchar *cmd_template;
	gchar *cmd;


	cmd_template = (gchar *) libslab_get_gconf_value (OPEN_IN_FILE_BROWSER_CMD_KEY);
	cmd = libslab_string_replace_once (cmd_template, "FILE_URI", priv->uri);

	libslab_spawn_command (cmd);

	g_free (cmd);
	g_free (cmd_template);
}

void
directory_tile_model_send_to (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

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
directory_tile_model_trash (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

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

	if (retval != GNOME_VFS_OK) {
		trash_uri_str = gnome_vfs_uri_to_string (trash_uri, GNOME_VFS_URI_HIDE_NONE);

		g_warning ("unable to move [%s] to the trash [%s]\n", priv->uri, trash_uri_str);

		g_free (trash_uri_str);
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (trash_uri);

	g_free (file_name);
}

void
directory_tile_model_delete (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

	GnomeVFSURI *src_uri;
	GList *list = NULL;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (priv->uri);

	list = g_list_append (list, src_uri);

	retval = gnome_vfs_xfer_delete_list (
		list, GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);

	if (retval != GNOME_VFS_OK)
		g_warning ("unable to delete [%s]\n", priv->uri);

	gnome_vfs_uri_unref (src_uri);
	g_list_free (list);
}

static void
this_class_init (DirectoryTileModelClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (DirectoryTileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

	GConfClient *client;


	priv->uri                      = NULL;

	priv->icon_id_attr             = NULL;
	priv->name_attr                = NULL;
	priv->is_local_attr            = NULL;
	priv->can_delete_attr          = NULL;

	priv->enable_delete_monitor_id = 0;

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, ENABLE_DELETE_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	g_object_unref (client);

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
	DirectoryTileModelPrivate *priv = PRIVATE (g_obj);

	GConfClient *client;


	g_free (priv->uri);

	g_object_unref (priv->icon_id_attr);
	g_object_unref (priv->name_attr);
	g_object_unref (priv->is_local_attr);
	g_object_unref (priv->can_delete_attr);

	gnome_vfs_volume_monitor_unref (volume_monitor);

	libslab_gconf_notify_remove (priv->enable_delete_monitor_id);

	client = gconf_client_get_default ();
	gconf_client_remove_dir (client, ENABLE_DELETE_KEY_DIR, NULL);
	g_object_unref (client);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
update_model (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

	gchar *basename;

	gchar *path;
	gchar *buf;
	gchar *tag_open_ptr  = NULL;
	gchar *tag_close_ptr = NULL;
	gchar *search_string = NULL;


	if (is_home_dir (this)) {
		tile_attribute_set_string (priv->icon_id_attr, "gnome-fs-home");
		tile_attribute_set_string (priv->name_attr, _("Home"));
	}
	else if (is_documents_dir (this)) {
		tile_attribute_set_string (priv->icon_id_attr, DEFAULT_ICON_ID);
		tile_attribute_set_string (priv->name_attr, _("Documents"));
	}
	else if (is_desktop_dir (this)) {
		tile_attribute_set_string (priv->icon_id_attr, "gnome-fs-desktop");
		tile_attribute_set_string (priv->name_attr, _("Desktop"));
	}
	else if (! libslab_strcmp (priv->uri, "file:///")) {
		tile_attribute_set_string (priv->icon_id_attr, "drive-harddisk");
		tile_attribute_set_string (priv->name_attr, _("File System"));
	}
	else if (! libslab_strcmp (priv->uri, "network:")) {
		tile_attribute_set_string (priv->icon_id_attr, "network-workgroup");
		tile_attribute_set_string (priv->name_attr, _("Network Servers"));
	}
	else if (g_str_has_prefix (priv->uri, "x-nautilus-search")) {
		tile_attribute_set_string (priv->icon_id_attr, "system-search");

		path = g_build_filename (
			g_get_home_dir (), ".nautilus", "searches", & priv->uri [21], NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_file_get_contents (path, & buf, NULL, NULL);

			if (buf) {
				tag_open_ptr  = strstr (buf, "<text>");
				tag_close_ptr = strstr (buf, "</text>");
			}

			if (tag_open_ptr && tag_close_ptr) {
				tag_close_ptr [0] = '\0';

				search_string = g_strdup_printf ("\"%s\"", & tag_open_ptr [6]);

				tag_close_ptr [0] = 'a';
			}

			g_free (buf);
		}

		if (search_string)
			tile_attribute_set_string (priv->name_attr, search_string);
		else
			tile_attribute_set_string (priv->name_attr, _("Search"));

		g_free (path);
	}
	else {
		tile_attribute_set_string (priv->icon_id_attr, DEFAULT_ICON_ID);

		path     = g_filename_from_uri (priv->uri, NULL, NULL);
		basename = g_path_get_basename (path);

		tile_attribute_set_string (priv->name_attr, basename);

		g_free (path);
		g_free (basename);
	}

	tile_attribute_set_boolean (priv->is_local_attr, dir_is_local (this));
}

static gboolean
is_home_dir (DirectoryTileModel *this)
{
	gchar    *uri_home;
	gboolean  retval;


	uri_home = g_filename_to_uri (g_get_home_dir (), NULL, NULL);
	retval = ! libslab_strcmp (PRIVATE (this)->uri, uri_home);
	g_free (uri_home);

	return retval;
}

static gboolean
is_documents_dir (DirectoryTileModel *this)
{
	gchar    *uri_docs;
	gchar    *path;
	gboolean  retval;


	path = g_build_filename (g_get_home_dir (), "Documents", NULL);
	uri_docs = g_filename_to_uri (path, NULL, NULL);

	retval = ! libslab_strcmp (PRIVATE (this)->uri, uri_docs);

	g_free (path);
	g_free (uri_docs);

	return retval;
}

static gboolean
is_desktop_dir (DirectoryTileModel *this)
{
	gchar    *uri_desk;
	gchar    *path;
	gboolean  retval;


	path = g_build_filename (g_get_home_dir (), "Desktop", NULL);
	uri_desk = g_filename_to_uri (path, NULL, NULL);

	retval = ! libslab_strcmp (PRIVATE (this)->uri, uri_desk);

	g_free (path);
	g_free (uri_desk);

	return retval;
}

static gboolean
dir_is_local (DirectoryTileModel *this)
{
	DirectoryTileModelPrivate *priv = PRIVATE (this);

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
volume_monitor_destroy_cb (gpointer data, GObject *g_obj)
{
	volume_monitor = NULL;
}

static void
uri_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	DirectoryTileModel        *this = DIRECTORY_TILE_MODEL (data);
	DirectoryTileModelPrivate *priv = PRIVATE (this);

	const gchar *uri;


	uri = tile_model_get_uri (TILE_MODEL (this));

	if (libslab_strcmp (uri, priv->uri)) {
		g_free (priv->uri);
		priv->uri = g_strdup (uri);
		update_model (this);
	}
}

static void
enable_delete_notify_cb (GConfClient *client, guint conn_id,
                         GConfEntry *entry, gpointer data)
{
	tile_attribute_set_boolean (
		PRIVATE (data)->can_delete_attr,
		GPOINTER_TO_INT (libslab_get_gconf_value (ENABLE_DELETE_KEY)));
}
