#include "tile-attribute.h"

#include "libslab-utils.h"

typedef struct {
	GValue              *value;
	TileAttributeStatus  status;
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
	PROP_STATUS
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

void
tile_attribute_set_boolean (TileAttribute *this, gboolean b)
{
	TileAttributePrivate *priv = PRIVATE (this);

	g_return_if_fail (G_VALUE_HOLDS (priv->value, G_TYPE_BOOLEAN));

	if (g_value_get_boolean (priv->value) == b)
		return;

	g_value_set_boolean (priv->value, b);
	g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_VALUE_PROP);
}

void
tile_attribute_set_int (TileAttribute *this, gint i)
{
	TileAttributePrivate *priv = PRIVATE (this);

	g_return_if_fail (G_VALUE_HOLDS (priv->value, G_TYPE_INT));

	if (g_value_get_int (priv->value) == i)
		return;

	g_value_set_int (priv->value, i);
	g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_VALUE_PROP);
}

void
tile_attribute_set_long (TileAttribute *this, glong l)
{
	TileAttributePrivate *priv = PRIVATE (this);

	g_return_if_fail (G_VALUE_HOLDS (priv->value, G_TYPE_LONG));

	if (g_value_get_long (priv->value) == l)
		return;

	g_value_set_long (priv->value, l);
	g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_VALUE_PROP);
}

void
tile_attribute_set_string (TileAttribute *this, const gchar *s)
{
	TileAttributePrivate *priv = PRIVATE (this);

	g_return_if_fail (G_VALUE_HOLDS (priv->value, G_TYPE_STRING));

	if (! libslab_strcmp (g_value_get_string (priv->value), s))
		return;

	g_value_reset (priv->value);
	g_value_set_string (priv->value, s);
	g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_VALUE_PROP);
}

void
tile_attribute_set_pointer (TileAttribute *this, gpointer p)
{
	TileAttributePrivate *priv = PRIVATE (this);

	g_return_if_fail (G_VALUE_HOLDS (priv->value, G_TYPE_POINTER));

	g_value_set_pointer (priv->value, p);
	g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_VALUE_PROP);
}

void
tile_attribute_set_status (TileAttribute *this, TileAttributeStatus status)
{
	TileAttributePrivate *priv = PRIVATE (this);

	if (priv->status != status) {
		priv->status = status;
		g_object_notify (G_OBJECT (this), TILE_ATTRIBUTE_STATUS_PROP);
	}
}

static void
this_class_init (TileAttributeClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *value_pspec;
	GParamSpec *status_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	value_pspec = g_param_spec_pointer (
		TILE_ATTRIBUTE_VALUE_PROP, TILE_ATTRIBUTE_VALUE_PROP,
		"the GValue object this attribute represents",
		G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	status_pspec = g_param_spec_int (
		TILE_ATTRIBUTE_STATUS_PROP, TILE_ATTRIBUTE_STATUS_PROP,
		"the TileAttributeStatus of this attribute",
		TILE_ATTRIBUTE_ACTIVE, TILE_ATTRIBUTE_HIDDEN, TILE_ATTRIBUTE_ACTIVE,
		G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_VALUE,  value_pspec);
	g_object_class_install_property (g_obj_class, PROP_STATUS, status_pspec);

	g_type_class_add_private (this_class, sizeof (TileAttributePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (TileAttribute *this)
{
	TileAttributePrivate *priv = PRIVATE (this);

	priv->value  = NULL;
	priv->status = TILE_ATTRIBUTE_ACTIVE;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	TileAttributePrivate *priv = PRIVATE (g_obj);

	switch (prop_id) {
		case PROP_VALUE:
			g_value_set_pointer (value, priv->value);
			break;

		case PROP_STATUS:
			g_value_set_int (value, priv->status);
			break;

		default:
			break;
	}
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	if (prop_id == PROP_STATUS)
		PRIVATE (g_obj)->status = g_value_get_int (value);
}

static void
finalize (GObject *g_obj)
{
	TileAttributePrivate *priv = PRIVATE (g_obj);

	g_value_unset (priv->value);
	g_free        (priv->value);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}
