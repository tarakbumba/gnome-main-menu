#include "tile-model.h"

typedef struct {
	TileAttribute *uri_attr;
} TileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_MODEL_TYPE, TileModelPrivate))

static void this_class_init (TileModelClass *);
static void this_init       (TileModel *);

static void finalize (GObject *);

static GObjectClass *this_parent_class = NULL;

GType
tile_model_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			G_TYPE_OBJECT, "TileModel",
			sizeof (TileModelClass), (GClassInitFunc) this_class_init,
			sizeof (TileModel), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

TileAttribute *
tile_model_get_uri_attr (TileModel *this)
{
	return PRIVATE (this)->uri_attr;
}

const gchar *
tile_model_get_uri (TileModel *this)
{
	return g_value_get_string (tile_attribute_get_value (PRIVATE (this)->uri_attr));
}

static void
this_class_init (TileModelClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (TileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (TileModel *this)
{
	PRIVATE (this)->uri_attr = tile_attribute_new (G_TYPE_STRING);
}

static void
finalize (GObject *g_obj)
{
	g_object_unref (PRIVATE (g_obj)->uri_attr);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}
