#include "tile-attribute.h"

typedef struct {
	GValue   *value;
	gboolean  active;
} TileAttributePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_ATTRIBUTE_TYPE, TileAttributePrivate))

static void this_class_init (TileAttributeClass *);
static void this_init       (TileAttribute *);

static void get_property (GObject *, guint, GValue *, GParamSpec *);
static void set_property (GObject *, guint, const GValue *, GParamSpec *);
static void finalize     (GObject *);

enum {
	PROP_0,
	PROP_VALUE,
	PROP_ACTIVE
};

static GObjectClass *this_parent_class = NULL;

GType
tile_attribute_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			G_TYPE_OBJECT, "TileAttribute",
			sizeof (TileAttributeClass), (GClassInitFunc) this_class_init,
			sizeof (TileAttribute), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

TileAttribute *
tile_attribute_new (GType type)
{
	TileAttribute        *this = g_object_new (TILE_ATTRIBUTE_TYPE, NULL);
	TileAttributePrivate *priv = PRIVATE (this);

	priv->value = g_new0 (GValue, 1);
	g_value_init (priv->value, type);

	return this;
}

GValue *
tile_attribute_get_value (TileAttribute *this)
{
	return PRIVATE (this)->value;
}

gboolean
tile_attribute_get_active (TileAttribute *this)
{
	return PRIVATE (this)->active;
}

void
tile_attribute_set_active (TileAttribute *this, gboolean active)
{
	g_object_set (G_OBJECT (this), TILE_ATTRIBUTE_ACTIVE_PROP, active, NULL);
}

static void
this_class_init (TileAttributeClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *value_pspec;
	GParamSpec *active_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	value_pspec = g_param_spec_pointer (
		TILE_ATTRIBUTE_VALUE_PROP, TILE_ATTRIBUTE_VALUE_PROP,
		"the GValue object this attribute represents",
		G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	active_pspec = g_param_spec_boolean (
		TILE_ATTRIBUTE_ACTIVE_PROP, TILE_ATTRIBUTE_ACTIVE_PROP,
		"TRUE if the attribute is active",
		TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_VALUE, value_pspec);
	g_object_class_install_property (g_obj_class, PROP_ACTIVE, active_pspec);

	g_type_class_add_private (this_class, sizeof (TileAttributePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (TileAttribute *this)
{
	TileAttributePrivate *priv = PRIVATE (this);

	priv->value  = NULL;
	priv->active = TRUE;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	TileAttributePrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_VALUE:
			g_value_set_pointer (value, priv->value);
			break;

		case PROP_ACTIVE:
			g_value_set_boolean (value, priv->active);
			break;

		default:
			break;
	}
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	if (prop_id == PROP_ACTIVE)
		PRIVATE (g_obj)->active = g_value_get_boolean (value);
}

static void
finalize (GObject *g_obj)
{
	TileAttributePrivate *priv = PRIVATE (g_obj);

	g_value_unset (priv->value);
	g_free        (priv->value);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}
