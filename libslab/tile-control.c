#include "tile-control.h"

typedef struct {
	TileAttribute          *attr_src;
	TileAttribute          *attr_dst;
	TileControlMappingFunc  mapping_func;
	gpointer                data;
} TileControlPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_CONTROL_TYPE, TileControlPrivate))

static void this_class_init (TileControlClass *);
static void this_init       (TileControl *);

static void finalize (GObject *);

static void map_values (TileControl *this);

static void source_notify_cb (GObject *, GParamSpec *, gpointer);

static GObjectClass *this_parent_class = NULL;

GType
tile_control_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			G_TYPE_OBJECT, "TileControl",
			sizeof (TileControlClass), (GClassInitFunc) this_class_init,
			sizeof (TileControl), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

TileControl *
tile_control_new (TileAttribute *source, TileAttribute *destination,
                  TileControlMappingFunc func, gpointer data)
{
	TileControl        *this;
	TileControlPrivate *priv;


	g_return_val_if_fail (source, NULL);
	g_return_val_if_fail (destination, NULL);

	this = g_object_new (TILE_CONTROL_TYPE, NULL);
	priv = PRIVATE (this);

	priv->attr_src     = g_object_ref (source);
	priv->attr_dst     = g_object_ref (destination);
	priv->mapping_func = func;
	priv->data         = data;

	map_values (this);

	g_signal_connect (
		G_OBJECT (priv->attr_src), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (source_notify_cb), this);

	return this;
}

static void
this_class_init (TileControlClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (TileControlPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (TileControl *this)
{
	TileControlPrivate *priv = PRIVATE (this);

	priv->attr_src     = NULL;
	priv->attr_dst     = NULL;
	priv->mapping_func = NULL;
	priv->data         = NULL;
}

static void
finalize (GObject *g_obj)
{
	TileControlPrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->attr_src);
	g_object_unref (priv->attr_dst);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
map_values (TileControl *this)
{
	TileControlPrivate *priv = PRIVATE (this);

	GValue *val_src;
	GValue *val_dst;


	val_src = tile_attribute_get_value (priv->attr_src);
	val_dst = tile_attribute_get_value (priv->attr_dst);

	if (priv->mapping_func)
		priv->mapping_func (val_src, val_dst, priv->data);
	else
		g_value_copy (val_src, val_dst);

	g_object_notify (G_OBJECT (priv->attr_dst), TILE_ATTRIBUTE_VALUE_PROP);
}

static void
source_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	map_values (TILE_CONTROL (data));
}
