#include "document-tile.h"

#include <glib/gi18n.h>

#include "file-tile-model.h"
#include "tile-button-view.h"
#include "tile-attribute.h"
#include "tile-control.h"
#include "libslab-utils.h"

typedef struct {
	FileTileModel  *model;
	TileButtonView *view;

	TileControl *icon_control;
	TileControl *name_hdr_control;
	TileControl *mtime_hdr_control;
} DocumentTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DOCUMENT_TILE_TYPE, DocumentTilePrivate))

static void this_class_init (DocumentTileClass *);
static void this_init       (DocumentTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void clicked_cb (GtkButton *, gpointer);

static void map_mtime_to_string (const GValue *, GValue *, gpointer);

static TileClass *this_parent_class = NULL;

GType
document_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "DocumentTile",
			sizeof (DocumentTileClass), (GClassInitFunc) this_class_init,
			sizeof (DocumentTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

DocumentTile *
document_tile_new (const gchar *uri)
{
	DocumentTile        *this;
	DocumentTilePrivate *priv;


	this = g_object_new (DOCUMENT_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = file_tile_model_new (uri);
	priv->view  = tile_button_view_new (2);

	priv->icon_control = tile_control_new (
		file_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view),
		NULL, NULL);
	priv->name_hdr_control = tile_control_new (
		file_tile_model_get_file_name_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 0),
		NULL, NULL);
	priv->mtime_hdr_control = tile_control_new (
		file_tile_model_get_mtime_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 1),
		map_mtime_to_string, NULL);

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);

	return this;
}

static void
this_class_init (DocumentTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (DocumentTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (DocumentTile *this)
{
	DocumentTilePrivate *priv = PRIVATE (this);

	priv->model             = NULL;
	priv->view              = NULL;
	priv->icon_control      = NULL;
	priv->name_hdr_control  = NULL;
	priv->mtime_hdr_control = NULL;
}

static void
finalize (GObject *g_obj)
{
	DocumentTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);
/*	g_object_unref (priv->view); */
	g_object_unref (priv->icon_control);
	g_object_unref (priv->name_hdr_control);
	g_object_unref (priv->mtime_hdr_control);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static GtkWidget *
get_widget (Tile *this)
{
	return GTK_WIDGET (PRIVATE (this)->view);
}

static gboolean
equals (Tile *this, gconstpointer that)
{
	const gchar *uri_this;
	const gchar *uri_that;

	uri_this = tile_model_get_uri (TILE_MODEL (PRIVATE (this)->model));

	if (IS_DOCUMENT_TILE (that))
		uri_that = tile_model_get_uri (TILE_MODEL (PRIVATE (that)->model));
	else
		uri_that = (const gchar *) that;

	return libslab_strcmp (uri_this, uri_that) == 0;
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	file_tile_model_open (PRIVATE (data)->model);

	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
map_mtime_to_string (const GValue *val_mtime, GValue *val_string, gpointer data)
{
	GDate *time;
	gchar *time_str;
	
	
	time = g_date_new ();
	g_date_set_time (time, g_value_get_long (val_mtime));

	time_str = g_new0 (gchar, 256);

	g_date_strftime (time_str, 256, _("Edited %m/%d/%Y"), time);
	g_date_free (time);

	g_value_set_string (val_string, time_str);
}
