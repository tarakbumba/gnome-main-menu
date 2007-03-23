#include "tile.h"

static void this_class_init (TileClass *);

enum {
	ACTION_TRIGGERED_SIGNAL,
	LAST_SIGNAL
};

static GObjectClass *this_parent_class = NULL;

static guint signals [LAST_SIGNAL] = { 0 };

GType
tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			G_TYPE_OBJECT, "Tile",
			sizeof (TileClass), (GClassInitFunc) this_class_init,
			sizeof (Tile), NULL, 0);

	return type_id;
}

GtkWidget *
tile_get_widget (Tile *this)
{
	TileClass *this_class = TILE_GET_CLASS (this);
	GtkWidget *widget = NULL;


	if (this_class->get_widget) {
		widget = this_class->get_widget (this);
		g_object_set_data (G_OBJECT (widget), "tile", this);
	}

	return widget;
}

Tile *
tile_get_tile_from_widget (GtkWidget *widget)
{
	gpointer tile = g_object_get_data (G_OBJECT (widget), "tile");

	return TILE (tile);
}

gboolean
tile_equals (Tile *this, gconstpointer that)
{
	TileClass *this_class = TILE_GET_CLASS (this);

	if (this_class->equals)
		return this_class->equals (this, that);

	return this == that;
}

void
tile_action_triggered (Tile *this, guint flags)
{
	g_signal_emit (this, signals [ACTION_TRIGGERED_SIGNAL], 0, flags);
}

static void
this_class_init (TileClass *this_class)
{
	this_class->get_widget = NULL;
	this_class->equals     = NULL;

	signals [ACTION_TRIGGERED_SIGNAL] = g_signal_new (
		TILE_ACTION_TRIGGERED_SIGNAL, G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, 0, NULL, NULL,
		g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);

	this_parent_class = g_type_class_peek_parent (this_class);
}
