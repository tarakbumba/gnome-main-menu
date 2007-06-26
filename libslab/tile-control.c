#include "tile-control.h"

typedef struct {
	TileAttribute          *attr_src;
	TileAttribute          *attr_dst;
	TileControlTriggerFunc  trigger_func;
	gpointer                trigger_data;
} TileControlPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_CONTROL_TYPE, TileControlPrivate))

static void this_class_init (TileControlClass *);
static void this_init       (TileControl *);

static void finalize (GObject *);

static void default_trigger (TileAttribute *, TileAttribute *, gpointer);

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
tile_control_new (TileAttribute *source, TileAttribute *destination)
{
	return tile_control_new_with_trigger_func (source, destination, default_trigger, NULL);
}

TileControl *
tile_control_new_with_trigger_func (TileAttribute *source, TileAttribute *destination,
                                    TileControlTriggerFunc trigger_func, gpointer trigger_data)
{
	TileControl        *this;
	TileControlPrivate *priv;


	g_return_val_if_fail (source, NULL);
	g_return_val_if_fail (destination, NULL);
	g_return_val_if_fail (trigger_func, NULL);

	this = g_object_new (TILE_CONTROL_TYPE, NULL);
	priv = PRIVATE (this);

	priv->attr_src     = g_object_ref (source);
	priv->attr_dst     = g_object_ref (destination);
	priv->trigger_func = trigger_func;
	priv->trigger_data = trigger_data;

	priv->trigger_func (priv->attr_src, priv->attr_dst, priv->trigger_data);

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
	priv->trigger_func = NULL;
	priv->trigger_data = NULL;
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
default_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	g_value_copy (tile_attribute_get_value (src), tile_attribute_get_value (dst));

	g_object_notify (G_OBJECT (dst), TILE_ATTRIBUTE_VALUE_PROP);
}

static void
source_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	TileControlPrivate *priv = PRIVATE (data);

	priv->trigger_func (priv->attr_src, priv->attr_dst, priv->trigger_data);
}
