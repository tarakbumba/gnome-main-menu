#include "file-tile-model.h"

#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "libslab-utils.h"

typedef struct {
	gchar  *file_name;
	gchar  *mime_type;
	time_t  mtime;
	gchar  *icon_id;

	TileAttribute *file_name_attr;
	TileAttribute *icon_id_attr;
	TileAttribute *mtime_attr;

	gchar *uri;

	GnomeVFSAsyncHandle     *handle;
	GnomeVFSMimeApplication *default_app;
} FileTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FILE_TILE_MODEL_TYPE, FileTileModelPrivate))

#define DEFAULT_ICON_ID  "gnome-fs-regular"
#define MAX_DESC_STR_LEN 1024

static void this_class_init (FileTileModelClass *);
static void this_init       (FileTileModel *);

static void finalize (GObject *);

static void update_model (FileTileModel *);

static void thumbnail_factory_destroy_cb (gpointer, GObject *);
static void uri_notify_cb (GObject *, GParamSpec *, gpointer);
static void file_info_cb (GnomeVFSAsyncHandle *, GList *, gpointer);

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

	priv->file_name      = NULL;
	priv->mime_type      = NULL;
	priv->mtime          = 0;
	priv->icon_id        = NULL;

	priv->file_name_attr = NULL;
	priv->icon_id_attr   = NULL;
	priv->mtime_attr     = NULL;

	priv->uri            = NULL;

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

	g_free (priv->icon_id);
	g_free (priv->file_name);
	g_free (priv->uri);
	g_free (priv->mime_type);

	g_object_unref (G_OBJECT (thumbnail_factory));

	g_object_unref (priv->file_name_attr);
	g_object_unref (priv->icon_id_attr);
	g_object_unref (priv->mtime_attr);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
update_model (FileTileModel *this)
{
	FileTileModelPrivate *priv = PRIVATE (this);

	gchar *basename;

	GList               *uri_list = NULL;


	g_free (priv->icon_id);
	g_free (priv->file_name);
	g_free (priv->mime_type);

	basename = g_path_get_basename (priv->uri);

	priv->file_name = gnome_vfs_unescape_string (basename, NULL);
	priv->icon_id   = g_strdup (DEFAULT_ICON_ID);
	priv->mime_type = g_strdup (gnome_vfs_get_mime_type_for_name (basename));
	priv->mtime     = 0;

	priv->default_app = gnome_vfs_mime_get_default_application (priv->mime_type);

	g_free (basename);

	g_value_set_string (tile_attribute_get_value (priv->file_name_attr), priv->file_name);
	g_value_set_string (tile_attribute_get_value (priv->icon_id_attr),   priv->icon_id);
	g_value_set_long   (tile_attribute_get_value (priv->mtime_attr),     priv->mtime);

	g_object_notify (G_OBJECT (priv->file_name_attr), TILE_ATTRIBUTE_VALUE_PROP);
	g_object_notify (G_OBJECT (priv->icon_id_attr),   TILE_ATTRIBUTE_VALUE_PROP);
	g_object_notify (G_OBJECT (priv->mtime_attr),     TILE_ATTRIBUTE_VALUE_PROP);

	uri_list = g_list_append (uri_list, gnome_vfs_uri_new (priv->uri));

	gnome_vfs_async_get_file_info (
		& priv->handle, uri_list, GNOME_VFS_FILE_INFO_GET_MIME_TYPE,
		GNOME_VFS_PRIORITY_DEFAULT, file_info_cb, this);

	g_list_free (uri_list);
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


	if (! results)
		return;

	result = (GnomeVFSGetFileInfoResult *) results->data;

	if (! (result && result->result == GNOME_VFS_OK))
		return;

	g_free (priv->file_name);
	priv->file_name = g_strdup (result->file_info->name);

	g_free (priv->mime_type);

	if (result->file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
		priv->mime_type = g_strdup (result->file_info->mime_type);
	else
		priv->mime_type = NULL;

	if (result->file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
		priv->mtime = result->file_info->mtime;
	else
		priv->mtime = 0;

	priv->default_app = gnome_vfs_mime_get_default_application (priv->mime_type);

	g_value_set_string (tile_attribute_get_value (priv->file_name_attr), priv->file_name);
	g_value_set_long   (tile_attribute_get_value (priv->mtime_attr),     priv->mtime);

	g_object_notify (G_OBJECT (priv->file_name_attr), TILE_ATTRIBUTE_VALUE_PROP);
	g_object_notify (G_OBJECT (priv->mtime_attr),     TILE_ATTRIBUTE_VALUE_PROP);
}
