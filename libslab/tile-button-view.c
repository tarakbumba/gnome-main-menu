#include "tile-button-view.h"

#include <string.h>
#include <stdlib.h>

#include "tile-view.h"
#include "double-click-detector.h"
#include "libslab-utils.h"

typedef struct {
	TileAttribute       *uri_attr;
	TileAttribute       *icon_attr;
	TileAttribute      **hdr_attrs;

	gchar               *uri;
	gchar               *icon_id;

	gboolean             debounce;
	DoubleClickDetector *dcd;
} TileButtonViewPrivate;

#define DEFAULT_ICON_SIZE  GTK_ICON_SIZE_DND
#define DEFAULT_ICON_ID    "stock_unknown"
#define WIDTH_HEIGHT_RATIO 6

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_BUTTON_VIEW_TYPE, TileButtonViewPrivate))

static void this_class_init (TileButtonViewClass *);
static void this_init       (TileButtonView *);

static void tile_view_interface_init (TileViewInterface *, gpointer);

static void finalize      (GObject *);
static void get_property  (GObject *, guint, GValue *, GParamSpec *);
static void set_property  (GObject *, guint, const GValue *, GParamSpec *);
static void drag_begin    (GtkWidget *, GdkDragContext *);
static void drag_data_get (GtkWidget *, GdkDragContext *,
                           GtkSelectionData *, guint, guint);
static void clicked       (GtkButton *);

static TileAttribute *get_attribute_by_id (TileView *, const gchar *);

static void uri_attr_notify_cb  (GObject *, GParamSpec *, gpointer);
static void icon_attr_notify_cb (GObject *, GParamSpec *, gpointer);
static void hdr_attr_notify_cb  (GObject *, GParamSpec *, gpointer);

enum {
	PROP_0,
	PROP_DEBOUNCE
};

static GtkButtonClass *this_parent_class;

GType
tile_button_view_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0)) {
		static const GInterfaceInfo interface_info = {
			(GInterfaceInitFunc) tile_view_interface_init, NULL, NULL
		};

		type_id = g_type_register_static_simple (
			GTK_TYPE_BUTTON, "TileButtonView",
			sizeof (TileButtonViewClass), (GClassInitFunc) this_class_init,
			sizeof (TileButtonView), (GInstanceInitFunc) this_init, 0);

		g_type_add_interface_static (type_id, TILE_VIEW_TYPE, & interface_info);
	}

	return type_id;
}

TileButtonView *
tile_button_view_new (gint n_hdrs)
{
	TileButtonView        *this;
	TileButtonViewPrivate *priv;

	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *alignment;

	gint i;


	this = g_object_new (TILE_BUTTON_VIEW_TYPE, NULL);
	priv = PRIVATE (this);

	gtk_button_set_relief (GTK_BUTTON (this), GTK_RELIEF_NONE);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (this), hbox);

	this->icon = gtk_image_new ();
	g_object_set (G_OBJECT (this->icon), "icon-size", DEFAULT_ICON_SIZE, NULL);
	gtk_box_pack_start (GTK_BOX (hbox), this->icon, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);
	gtk_box_pack_start (GTK_BOX (hbox), alignment, TRUE, TRUE, 0);

	this->n_hdrs    = n_hdrs;
	this->headers   = g_new0 (GtkWidget *, this->n_hdrs + 1);
	priv->hdr_attrs = g_new0 (TileAttribute *, this->n_hdrs + 1);

	for (i = 0; i < n_hdrs; ++i) {
		this->headers [i] = gtk_label_new (NULL);
		gtk_label_set_ellipsize (GTK_LABEL (this->headers [i]), PANGO_ELLIPSIZE_END);
		gtk_misc_set_alignment (GTK_MISC (this->headers [i]), 0.0, 0.5);

		if (i > 0)
			gtk_widget_modify_fg (
				this->headers [i], GTK_STATE_NORMAL,
				& this->headers [i]->style->fg [GTK_STATE_INSENSITIVE]);

		gtk_box_pack_start (GTK_BOX (vbox), this->headers [i], TRUE, TRUE, 0);

		priv->hdr_attrs [i] = tile_attribute_new (G_TYPE_STRING);
		g_object_set_data (G_OBJECT (priv->hdr_attrs [i]), "header-index", GINT_TO_POINTER (i));

		g_signal_connect (
			G_OBJECT (priv->hdr_attrs [i]), "notify", G_CALLBACK (hdr_attr_notify_cb), this);
	}

	priv->uri_attr  = tile_attribute_new (G_TYPE_STRING);
	priv->icon_attr = tile_attribute_new (G_TYPE_STRING);

	g_signal_connect (
		G_OBJECT (priv->uri_attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (uri_attr_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->icon_attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (icon_attr_notify_cb), this);

	return this;
}

TileAttribute *
tile_button_view_get_icon_id_attr (TileButtonView *this)
{
	return PRIVATE (this)->icon_attr;
}

TileAttribute *
tile_button_view_get_header_text_attr (TileButtonView *this, gint index)
{
	g_return_val_if_fail (0 <= index && index < this->n_hdrs, NULL);

	return PRIVATE (this)->hdr_attrs [index];
}

static void
this_class_init (TileButtonViewClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);
	GtkButtonClass *button_class = GTK_BUTTON_CLASS (this_class);

	GParamSpec *debounce_pspec;


	g_obj_class->finalize       = finalize;
	g_obj_class->get_property   = get_property;
	g_obj_class->set_property   = set_property;
	widget_class->drag_begin    = drag_begin;
	widget_class->drag_data_get = drag_data_get;
	button_class->clicked       = clicked;

	debounce_pspec = g_param_spec_boolean (
		TILE_BUTTON_VIEW_DEBOUNCE_PROP, TILE_BUTTON_VIEW_DEBOUNCE_PROP,
		"TRUE if the view should suppress double clicks", TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_DEBOUNCE, debounce_pspec);

	g_type_class_add_private (this_class, sizeof (TileButtonViewPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
tile_view_interface_init (TileViewInterface *iface, gpointer data)
{
	iface->get_attribute_by_id = get_attribute_by_id;
}

static void
this_init (TileButtonView *this)
{
	TileButtonViewPrivate *priv = PRIVATE (this);

	priv->uri_attr   = NULL;
	priv->icon_attr  = NULL;
	priv->hdr_attrs  = NULL;

	priv->uri        = NULL;
	priv->icon_id    = NULL;

	priv->debounce   = TRUE;
	priv->dcd        = double_click_detector_new ();
}

static void
finalize (GObject *g_obj)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (g_obj);
	TileButtonViewPrivate *priv = PRIVATE (this);

	gint i;


	if (G_IS_OBJECT (priv->uri_attr))
		g_object_unref (G_OBJECT (priv->uri_attr));

	if (G_IS_OBJECT (priv->icon_attr))
		g_object_unref (G_OBJECT (priv->icon_attr));

	for (i = 0; i < this->n_hdrs; ++i)
		if (G_IS_OBJECT (priv->hdr_attrs [i]))
			g_object_unref (G_OBJECT (priv->hdr_attrs [i]));

	g_free (priv->hdr_attrs);
	g_free (priv->uri);
	g_free (priv->icon_id);

	if (G_IS_OBJECT (priv->dcd))
		g_object_unref (priv->dcd);

	g_free (this->headers);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
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
	if (prop_id == PROP_DEBOUNCE)
		PRIVATE (g_obj)->debounce = g_value_get_boolean (value);
}

static void
drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	TileButtonView *this = TILE_BUTTON_VIEW (widget);

	GTK_WIDGET_CLASS (this_parent_class)->drag_begin (widget, context);

	if (! (this->icon && GTK_IS_IMAGE (this->icon)))
		return;

	switch (GTK_IMAGE (this->icon)->storage_type) {
		case GTK_IMAGE_PIXBUF:
			if (GTK_IMAGE (this->icon)->data.pixbuf.pixbuf)
				gtk_drag_set_icon_pixbuf (context, GTK_IMAGE (this->icon)->data.pixbuf.pixbuf, 0, 0);

			break;

		case GTK_IMAGE_ICON_NAME:
			if (GTK_IMAGE (this->icon)->data.name.pixbuf)
				gtk_drag_set_icon_pixbuf (context, GTK_IMAGE (this->icon)->data.name.pixbuf, 0, 0);

			break;

		default:
			break;
	}
}

static void
drag_data_get (GtkWidget *widget, GdkDragContext *context,
               GtkSelectionData *data, guint info, guint time)
{
	TileButtonViewPrivate *priv = PRIVATE (widget);

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
	TileButtonView        *this = TILE_BUTTON_VIEW (widget);
	TileButtonViewPrivate *priv = PRIVATE (this);

	guint32  time;
	gboolean debounce;


	time     = libslab_get_current_time_millis ();
	debounce =
		priv->debounce &&
		double_click_detector_is_double_click (priv->dcd, time, TRUE);

	if (debounce)
		g_signal_stop_emission_by_name (widget, "clicked");
}

static TileAttribute *
get_attribute_by_id (TileView *this, const gchar *id)
{
	g_return_val_if_fail (id, NULL);

	if (! strcmp (id, TILE_BUTTON_VIEW_ICON_ID_ATTR))
		return tile_button_view_get_icon_id_attr (TILE_BUTTON_VIEW (this));
	else if (g_str_has_prefix (id, TILE_BUTTON_VIEW_HDR_TXT_ATTR_PREFIX))
		return tile_button_view_get_header_text_attr (
			TILE_BUTTON_VIEW (this),
			atoi (& id [strlen (TILE_BUTTON_VIEW_HDR_TXT_ATTR_PREFIX)]));
	else
		g_return_val_if_reached (NULL);

	return NULL;
}

static void
uri_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (data);
	TileButtonViewPrivate *priv = PRIVATE (this);

	const GValue *value;
	const gchar  *uri;


	value = tile_attribute_get_value (priv->uri_attr);

	if (! (value && G_VALUE_HOLDS (value, G_TYPE_STRING)))
		return;

	uri = g_value_get_string (value);

	if (! libslab_strcmp (uri, priv->uri))
		return;

	g_free (priv->uri);
	priv->uri = g_strdup (uri);

	if (priv->uri) {
		gtk_drag_source_set (
			GTK_WIDGET (this), GDK_BUTTON1_MASK, NULL, 0,
			GDK_ACTION_COPY | GDK_ACTION_MOVE);

		gtk_drag_source_add_uri_targets (GTK_WIDGET (this));
	}
	else
		gtk_drag_source_unset (GTK_WIDGET (this));
}

static void
icon_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (data);
	TileButtonViewPrivate *priv = PRIVATE (this);

	const GValue *value;
	const gchar  *icon_id;


	value = tile_attribute_get_value (priv->icon_attr);

	if (! (value && G_VALUE_HOLDS (value, G_TYPE_STRING)))
		return;

	icon_id = g_value_get_string (value);

	if (! icon_id)
		gtk_widget_hide (this->icon);
	else if (libslab_strcmp (priv->icon_id, icon_id)) {
		priv->icon_id = g_strdup (icon_id);
		libslab_gtk_image_set_by_id (GTK_IMAGE (this->icon), priv->icon_id);
		gtk_widget_show (this->icon);
	}
}

static void
hdr_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (data);
	TileButtonViewPrivate *priv = PRIVATE (this);

	const GValue *value;
	const gchar  *text;

	gint index;


	index = GPOINTER_TO_INT (g_object_get_data (g_obj, "header-index"));
	value = tile_attribute_get_value (priv->hdr_attrs [index]);

	if (! (value && G_VALUE_HOLDS (value, G_TYPE_STRING)))
		return;

	text = g_value_get_string (value);

	if (! text)
		gtk_widget_hide (this->headers [index]);
	else {
		gtk_label_set_text (GTK_LABEL (this->headers [index]), text);
		gtk_widget_show (GTK_WIDGET (this->headers [index]));
	}
}





#if 0
#include "tile-button-view.h"

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
#endif
