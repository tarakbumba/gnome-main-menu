#include "document-tile.h"

#include <glib/gi18n.h>

#include "file-tile-model.h"
#include "nameplate-tile-view.h"
#include "tile-attribute.h"
#include "tile-control.h"

typedef struct {
	FileTileModel     *model;
	NameplateTileView *view;

	TileControl *icon_control;
	TileControl *name_hdr_control;
	TileControl *mtime_hdr_control;
} DocumentTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DOCUMENT_TILE_TYPE, DocumentTilePrivate))

static void this_class_init (DocumentTileClass *);
static void this_init       (DocumentTile *);

static void finalize (GObject *);

static void primary_action (Tile *);

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

GtkWidget *
document_tile_new (const gchar *uri)
{
	GtkWidget           *this;
	DocumentTilePrivate *priv;

	TileAttribute *icon_attr;
	TileAttribute *hdr_attr;
	TileAttribute *subhdr_attr;


	g_printf ("%s (%s)\n", G_STRFUNC, uri);

	this = GTK_WIDGET (g_object_new (DOCUMENT_TILE_TYPE, NULL));
	priv = PRIVATE (this);

	priv->model = file_tile_model_new (uri);
	priv->view  = nameplate_tile_view_new (2);

	icon_attr   = tile_attribute_new (G_TYPE_STRING);
	hdr_attr    = tile_attribute_new (G_TYPE_STRING);
	subhdr_attr = tile_attribute_new (G_TYPE_STRING);

	nameplate_tile_view_set_icon_attr   (priv->view, icon_attr);
	nameplate_tile_view_set_header_attr (priv->view, hdr_attr, 0);
	nameplate_tile_view_set_header_attr (priv->view, subhdr_attr, 1);

	priv->icon_control      = tile_control_new (TILE_MODEL (priv->model), FILE_TILE_MODEL_ICON_ID_PROP, icon_attr, NULL, NULL);
	priv->name_hdr_control  = tile_control_new (TILE_MODEL (priv->model), FILE_TILE_MODEL_FILE_NAME_PROP, hdr_attr, NULL, NULL);
	priv->mtime_hdr_control = tile_control_new (TILE_MODEL (priv->model), FILE_TILE_MODEL_MTIME_PROP, subhdr_attr, map_mtime_to_string, NULL);

	return this;
}

static void
this_class_init (DocumentTileClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize       = finalize;
	TILE_CLASS     (this_class)->primary_action = primary_action;

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

static void
primary_action (Tile *this)
{
	file_tile_model_open (PRIVATE (this)->model);

	tile_action_triggered (this, TILE_ACTION_LAUNCHES_APP);
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
