#include "tile.h"

#include "double-click-detector.h"
#include "libslab-utils.h"

typedef struct {
	gboolean debounce;
	DoubleClickDetector *dcd;

	gchar   *uri;
	GtkMenu *context_menu;
} TilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_TYPE, TilePrivate))

static void this_class_init (TileClass *);
static void this_init       (Tile *);

static void get_property (GObject *, guint, GValue *, GParamSpec *);
static void set_property (GObject *, guint, const GValue *, GParamSpec *);
static void finalize     (GObject *);

static gboolean button_press  (GtkWidget *, GdkEventButton *);
static void     drag_data_get (GtkWidget *, GdkDragContext *,
                               GtkSelectionData *, guint, guint);

static void clicked (GtkButton *);

enum {
	PROP_0,
	PROP_DEBOUNCE,
	PROP_URI,
	PROP_CONTEXT_MENU
};

enum {
	ACTION_TRIGGERED_SIGNAL,
	LAST_SIGNAL
};

static GtkButtonClass *this_parent_class = NULL;

static guint signals [LAST_SIGNAL] = { 0 };

GType
tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			GTK_TYPE_BUTTON, "Tile",
			sizeof (TileClass), (GClassInitFunc) this_class_init,
			sizeof (Tile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

void
tile_action_triggered (Tile *this, guint flags)
{
	g_signal_emit (this, signals [ACTION_TRIGGERED_SIGNAL], 0, flags);
}

gint
tile_compare (gconstpointer a, gconstpointer b)
{
	if (IS_TILE (a) && IS_TILE (b))
		return libslab_strcmp (PRIVATE (a)->uri, PRIVATE (b)->uri);

	return a - b;
}

gboolean
tile_equals (gconstpointer a, gconstpointer b)
{
	if (IS_TILE (a) && IS_TILE (b))
		return libslab_strcmp (PRIVATE (a)->uri, PRIVATE (b)->uri) == 0;

	if (IS_TILE (a))
		return libslab_strcmp (PRIVATE (a)->uri, (const gchar *) b) == 0;

	if (IS_TILE (b))
		return libslab_strcmp (PRIVATE (b)->uri, (const gchar *) a) == 0;

	return a == b;
}

static void
this_class_init (TileClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);
	GtkButtonClass *button_class = GTK_BUTTON_CLASS (this_class);

	GParamSpec *debounce_pspec;
	GParamSpec *uri_pspec;
	GParamSpec *context_menu_pspec;


	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	widget_class->button_press_event = button_press;
	widget_class->drag_data_get      = drag_data_get;

	button_class->clicked = clicked;

	this_class->primary_action   = NULL;
	this_class->action_triggered = NULL;

	debounce_pspec = g_param_spec_boolean (
		TILE_DEBOUNCE_PROP, TILE_DEBOUNCE_PROP,
		"TRUE if the tile should suppress double clicks", TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	uri_pspec = g_param_spec_string (
		TILE_URI_PROP, TILE_URI_PROP,
		"the uri associated with the tile", NULL,
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	context_menu_pspec = g_param_spec_object (
		TILE_CONTEXT_MENU_PROP, TILE_CONTEXT_MENU_PROP,
		"the context menu associated with the tile", GTK_TYPE_MENU,
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_DEBOUNCE,     debounce_pspec);
	g_object_class_install_property (g_obj_class, PROP_URI,          uri_pspec);
	g_object_class_install_property (g_obj_class, PROP_CONTEXT_MENU, context_menu_pspec);

	signals [ACTION_TRIGGERED_SIGNAL] = g_signal_new (
		TILE_ACTION_TRIGGERED_SIGNAL, G_TYPE_FROM_CLASS (this_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, 0, NULL, NULL,
		g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);

	g_type_class_add_private (this_class, sizeof (TilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (Tile *this)
{
	TilePrivate *priv = PRIVATE (this);

	priv->debounce = TRUE;
	priv->dcd      = double_click_detector_new ();

	priv->uri          = NULL;
	priv->context_menu = NULL;

	gtk_button_set_relief (GTK_BUTTON (this), GTK_RELIEF_NONE);
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	if (prop_id == PROP_DEBOUNCE)
		g_value_set_boolean (value, PRIVATE (g_obj)->debounce);
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Tile        *this = TILE    (g_obj);
	TilePrivate *priv = PRIVATE (this);


	switch (prop_id) {
		case PROP_DEBOUNCE:
			priv->debounce = g_value_get_boolean (value);
			break;

		case PROP_URI:
			g_free (priv->uri);
			priv->uri = g_value_dup_string (value);

			if (priv->uri) {
				gtk_drag_source_set (
					GTK_WIDGET (this), GDK_BUTTON1_MASK, NULL, 0,
					GDK_ACTION_COPY | GDK_ACTION_MOVE);

				gtk_drag_source_add_uri_targets (GTK_WIDGET (this));
			}
			else
				gtk_drag_source_unset (GTK_WIDGET (this));

			break;

		case PROP_CONTEXT_MENU:
			if (GTK_IS_WIDGET (priv->context_menu))
				gtk_widget_destroy (GTK_WIDGET (priv->context_menu));

			priv->context_menu = GTK_MENU (g_value_dup_object (value));

			break;

		default:
			break;
	}
}

static void
finalize (GObject *g_obj)
{
	TilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->dcd);
	g_free         (priv->uri);

	if (GTK_IS_WIDGET (priv->context_menu))
		gtk_widget_destroy (GTK_WIDGET (priv->context_menu));

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static gboolean
button_press (GtkWidget *widget, GdkEventButton *event)
{
	TilePrivate *priv = PRIVATE (widget);

	if (event->button != 3 || ! GTK_IS_MENU (priv->context_menu))
		return FALSE;

	gtk_menu_popup (
		priv->context_menu, NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static void
drag_data_get (GtkWidget *widget, GdkDragContext *context,
               GtkSelectionData *data, guint info, guint time)
{
	TilePrivate *priv = PRIVATE (widget);

	gchar *uris [2];


	if (priv->uri) {
		uris [0] = priv->uri;
		uris [1] = NULL;

		gtk_selection_data_set_uris (data, uris);
	}
}

static void
clicked (GtkButton *widget)
{
	Tile        *this = TILE    (widget);
	TilePrivate *priv = PRIVATE (this);

	guint32  time;
	gboolean debounce;


	time     = libslab_get_current_time_millis ();
	debounce =
		priv->debounce &&
		double_click_detector_is_double_click (priv->dcd, time, TRUE);

	if (! debounce && TILE_GET_CLASS (this)->primary_action)
		TILE_GET_CLASS (this)->primary_action (this);
}
