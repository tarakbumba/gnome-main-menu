#include "file-tile-model.h"

#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "libslab-utils.h"
#include "bookmark-agent.h"

typedef struct {
	gchar *uri;
	gchar *mime_type;

	TileAttribute *file_name_attr;
	TileAttribute *icon_id_attr;
	TileAttribute *mtime_attr;
	TileAttribute *app_attr;

	GnomeVFSAsyncHandle     *handle;
	GnomeVFSMimeApplication *default_app;
} FileTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FILE_TILE_MODEL_TYPE, FileTileModelPrivate))

#define OPEN_IN_FILE_BROWSER_CMD_KEY "/desktop/gnome/applications/main-menu/file-area/file_mgr_open_cmd"

#define DEFAULT_ICON_ID  "gnome-fs-regular"
#define MAX_DESC_STR_LEN 1024

static void this_class_init (FileTileModelClass *);
static void this_init       (FileTileModel *);

static void finalize (GObject *);

static void update_model      (FileTileModel *);
static void update_mime_type  (FileTileModel *, const gchar *);

static void thumbnail_factory_destroy_cb (gpointer, GObject *);
static void uri_notify_cb                (GObject *, GParamSpec *, gpointer);
static void file_info_cb                 (GnomeVFSAsyncHandle *, GList *, gpointer);

static GnomeThumbnailFactory *thumbnail_factory = NULL;

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
	
	
	this = g_object_new (FILE_TILE_MODEL_TYPE, TILE_MODEL_URI_PROP, uri, NULL);
	priv = PRIVATE (this);

	priv->file_name_attr = tile_attribute_new (G_TYPE_STRING);
	priv->icon_id_attr   = tile_attribute_new (G_TYPE_STRING);
	priv->mtime_attr     = tile_attribute_new (G_TYPE_LONG);
	priv->app_attr       = tile_attribute_new (G_TYPE_POINTER);

	priv->uri = g_strdup (tile_model_get_uri (TILE_MODEL (this)));

	update_model (this);

	g_signal_connect (
		this, "notify::" TILE_MODEL_URI_PROP, G_CALLBACK (uri_notify_cb), this);

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

	gchar *cmd;

	filename = g_filename_from_uri (priv->uri, NULL, NULL);
	dirname  = g_path_get_dirname (filename);
	uri      = g_filename_to_uri (dirname, NULL, NULL);

	if (! uri)
		g_warning ("error getting dirname for [%s]\n", priv->uri);
	else {
		cmd = libslab_string_replace_once (
			(gchar *) libslab_get_gconf_value (OPEN_IN_FILE_BROWSER_CMD_KEY),
			"FILE_URI", uri);

		libslab_spawn_command (cmd);

		g_free (cmd);
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
	gchar *src_uri_str;

	BookmarkAgent *rct_docs_agent;

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
			src_uri_str = priv->uri;
			priv->uri   = gnome_vfs_uri_to_string (dst_uri, GNOME_VFS_URI_HIDE_NONE);

			update_model (this);

			rct_docs_agent = bookmark_agent_get_instance (BOOKMARK_STORE_RECENT_DOCS);
			bookmark_agent_move_item (rct_docs_agent, src_uri_str, priv->uri);
			g_object_unref (rct_docs_agent);

			g_free (src_uri_str);
		}
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dst_uri);

	g_free (dirname);
	g_free (dst_path);
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

	priv->uri            = NULL;
	priv->mime_type      = NULL;

	priv->file_name_attr = NULL;
	priv->icon_id_attr   = NULL;
	priv->mtime_attr     = NULL;
	priv->app_attr       = NULL;

	priv->handle         = NULL;
	priv->default_app    = NULL;

	if (! thumbnail_factory) {
		thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
		g_object_weak_ref (G_OBJECT (thumbnail_factory), thumbnail_factory_destroy_cb, NULL);
	}
	else
		g_object_ref (G_OBJECT (thumbnail_factory));
}

static void
finalize (GObject *g_obj)
{
	FileTileModelPrivate *priv = PRIVATE (g_obj);

	gnome_vfs_async_cancel (priv->handle);

	g_free (priv->uri);
	g_free (priv->mime_type);

	g_object_unref (G_OBJECT (thumbnail_factory));

	g_object_unref (priv->file_name_attr);
	g_object_unref (priv->icon_id_attr);
	g_object_unref (priv->mtime_attr);
	g_object_unref (priv->app_attr);

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

	uri      = gnome_vfs_uri_new (priv->uri);
	uri_list = g_list_append (uri_list, uri);

	gnome_vfs_async_get_file_info (
		& priv->handle, uri_list, GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
		GNOME_VFS_PRIORITY_DEFAULT, file_info_cb, this);

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

static void
thumbnail_factory_destroy_cb (gpointer data, GObject *g_obj)
{
	thumbnail_factory = NULL;
}

static void
uri_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	FileTileModelPrivate *priv = PRIVATE (data);

	const gchar *uri;


	uri = tile_model_get_uri (TILE_MODEL (data));

	if (libslab_strcmp (uri, priv->uri)) {
		g_free (priv->uri);
		priv->uri = g_strdup (uri);
		update_model (FILE_TILE_MODEL (data));
	}
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
