#include "tile-model.h"

typedef struct {
	gchar *uri;
} TileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_MODEL_TYPE, TileModelPrivate))

static void this_class_init (TileModelClass *);
static void this_init       (TileModel *);

static void get_property (GObject *, guint, GValue *, GParamSpec *);
static void set_property (GObject *, guint, const GValue *, GParamSpec *);
static void finalize     (GObject *);

enum {
	PROP_0,
	PROP_URI
};

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

const gchar *
tile_model_get_uri (TileModel *this)
{
	return PRIVATE (this)->uri;
}

void
tile_model_set_uri (TileModel *this, const gchar *uri)
{
	g_object_set (G_OBJECT (this), TILE_MODEL_URI_PROP, uri, NULL);
}

static void
this_class_init (TileModelClass *this_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) this_class;

	GParamSpec *uri_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	uri_pspec = g_param_spec_string (
		TILE_MODEL_URI_PROP, TILE_MODEL_URI_PROP, "the URI associated with the resourse",
		NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_URI, uri_pspec);

	g_type_class_add_private (this_class, sizeof (TileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (TileModel *this)
{
	PRIVATE (this)->uri = NULL;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	if (prop_id == PROP_URI)
		g_value_set_string (value, PRIVATE (g_obj)->uri);
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	TileModelPrivate *priv = PRIVATE (g_obj);

	if (prop_id == PROP_URI) {
		g_free (priv->uri);
		priv->uri = g_value_dup_string (value);
	}
}

static void
finalize (GObject *g_obj)
{
	g_free (PRIVATE (g_obj)->uri);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}
