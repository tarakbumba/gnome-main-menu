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
	TileAttribute       *tooltip_attr;

	gchar               *uri;
	gchar               *icon_id;

	GtkMenu             *context_menu;
	GtkVBox             *hdr_box;
	GtkTooltips         *tooltip;

	gboolean             debounce;
	DoubleClickDetector *dcd;
} TileButtonViewPrivate;

#define DEFAULT_ICON_SIZE GTK_ICON_SIZE_DND
#define DEFAULT_ICON_ID   "stock_unknown"

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TILE_BUTTON_VIEW_TYPE, TileButtonViewPrivate))

static void this_class_init (TileButtonViewClass *);
static void this_init       (TileButtonView *);

static void tile_view_interface_init (TileViewInterface *, gpointer);

static void     finalize       (GObject *);
static void     get_property   (GObject *, guint, GValue *, GParamSpec *);
static void     set_property   (GObject *, guint, const GValue *, GParamSpec *);
static void     drag_begin     (GtkWidget *, GdkDragContext *);
static void     drag_data_get  (GtkWidget *, GdkDragContext *, GtkSelectionData *, guint, guint);
static gboolean button_release (GtkWidget *, GdkEventButton *);

static TileAttribute *get_attribute_by_id (TileView *, const gchar *);

static void uri_attr_notify_cb     (GObject *, GParamSpec *, gpointer);
static void icon_attr_notify_cb    (GObject *, GParamSpec *, gpointer);
static void hdr_attr_notify_cb     (GObject *, GParamSpec *, gpointer);
static void tooltip_attr_notify_cb (GObject *, GParamSpec *, gpointer);

static void edit_header_entry_activate_cb (GtkEntry *, gpointer);

enum {
	PROP_0,
	PROP_DEBOUNCE,
	PROP_ICON_SIZE
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

	priv->hdr_box = GTK_VBOX (gtk_vbox_new (FALSE, 0));
	alignment = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (priv->hdr_box));
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

		gtk_box_pack_start (GTK_BOX (priv->hdr_box), this->headers [i], TRUE, TRUE, 0);

		priv->hdr_attrs [i] = tile_attribute_new (G_TYPE_STRING);
		g_object_set_data (G_OBJECT (priv->hdr_attrs [i]), "header-index", GINT_TO_POINTER (i));

		g_signal_connect (
			G_OBJECT (priv->hdr_attrs [i]), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
			G_CALLBACK (hdr_attr_notify_cb), this);
	}

	priv->uri_attr     = tile_attribute_new (G_TYPE_STRING);
	priv->icon_attr    = tile_attribute_new (G_TYPE_STRING);
	priv->tooltip_attr = tile_attribute_new (G_TYPE_STRING);

	g_signal_connect (
		G_OBJECT (priv->uri_attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (uri_attr_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->icon_attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (icon_attr_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->tooltip_attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (tooltip_attr_notify_cb), this);

	return this;
}

void
tile_button_view_add_context_menu (TileButtonView *this, GtkMenu *menu)
{
	PRIVATE (this)->context_menu = menu;
}

TileAttribute *
tile_button_view_get_uri_attr (TileButtonView *this)
{
	return PRIVATE (this)->uri_attr;
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

TileAttribute *
tile_button_view_get_tooltip_attr (TileButtonView *this)
{
	return PRIVATE (this)->tooltip_attr;
}

void
tile_button_view_activate_header_edit (TileButtonView *this, gint index)
{
	TileButtonViewPrivate *priv = PRIVATE (this);

	GtkWidget *entry;


	g_return_if_fail (0 <= index && index < this->n_hdrs);

	entry = gtk_entry_new ();
	gtk_entry_set_text (
		GTK_ENTRY (entry),
		gtk_label_get_text (GTK_LABEL (this->headers [index])));
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

	gtk_widget_destroy (this->headers [index]);

	gtk_container_add_with_properties (
		GTK_CONTAINER (priv->hdr_box), entry,
		"position", index, NULL);

	g_signal_connect (
		G_OBJECT (entry), "activate",
		G_CALLBACK (edit_header_entry_activate_cb), this);

	gtk_widget_show (entry);
	gtk_widget_grab_focus (entry);
}

static void
this_class_init (TileButtonViewClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	GParamSpec *debounce_pspec;


	g_obj_class->finalize              = finalize;
	g_obj_class->get_property          = get_property;
	g_obj_class->set_property          = set_property;
	widget_class->drag_begin           = drag_begin;
	widget_class->drag_data_get        = drag_data_get;
	widget_class->button_release_event = button_release;

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

	priv->uri_attr     = NULL;
	priv->icon_attr    = NULL;
	priv->hdr_attrs    = NULL;
	priv->tooltip_attr = NULL;

	priv->uri          = NULL;
	priv->icon_id      = NULL;

	priv->context_menu = NULL;
	priv->hdr_box      = NULL;
	priv->tooltip      = NULL;

	priv->debounce     = TRUE;
	priv->dcd          = double_click_detector_new ();
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

	if (G_IS_OBJECT (priv->tooltip_attr))
		g_object_unref (G_OBJECT (priv->tooltip_attr));

	g_free (priv->hdr_attrs);
	g_free (priv->uri);
	g_free (priv->icon_id);

	if (GTK_IS_WIDGET (priv->context_menu))
		gtk_widget_destroy (GTK_WIDGET (priv->context_menu));

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

static gboolean
button_release (GtkWidget *widget, GdkEventButton *event)
{
	TileButtonViewPrivate *priv = PRIVATE (widget);

	guint32  time;
	gboolean debounce;

	
	switch (event->button) {
		case 1:
			time = libslab_get_current_time_millis ();
			debounce =
				priv->debounce &&
				double_click_detector_is_double_click (priv->dcd, time, TRUE);

			if (debounce)
				return TRUE;

			break;

		case 3:
			if (GTK_IS_MENU (priv->context_menu)) {
				gtk_menu_popup (
					priv->context_menu, NULL, NULL, NULL, NULL,
					event->button, event->time);

				return TRUE;
			}

			break;

		default:
			break;
	}

	return GTK_WIDGET_CLASS (this_parent_class)->button_release_event (widget, event);
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

	if (! text) {
		gtk_widget_destroy (this->headers [index]);
		this->headers [index] = NULL;
	}
	else {
		if (! this->headers [index]) {
			this->headers [index] = gtk_label_new (NULL);
			gtk_misc_set_alignment (GTK_MISC (this->headers [index]), 0.0, 0.5);

			gtk_container_add_with_properties (
				GTK_CONTAINER (priv->hdr_box), this->headers [index],
				"position", index, NULL);
		}

		gtk_label_set_text (GTK_LABEL (this->headers [index]), text);
		gtk_widget_show (GTK_WIDGET (this->headers [index]));
	}
}

static void
tooltip_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (data);
	TileButtonViewPrivate *priv = PRIVATE (this);

	const gchar *tooltip_text;


	tooltip_text = g_value_get_string (tile_attribute_get_value (priv->tooltip_attr));

	if (tooltip_text) {
		if (! priv->tooltip)
			priv->tooltip = gtk_tooltips_new ();

		gtk_tooltips_set_tip (
			priv->tooltip, GTK_WIDGET (this), tooltip_text, tooltip_text);
		gtk_tooltips_enable (priv->tooltip);
	}
	else
		if (GTK_IS_TOOLTIPS (priv->tooltip))
			gtk_tooltips_disable (priv->tooltip);
}

static void
edit_header_entry_activate_cb (GtkEntry *entry, gpointer data)
{
	TileButtonView        *this = TILE_BUTTON_VIEW (data);
	TileButtonViewPrivate *priv = PRIVATE (this);

	const gchar *entry_text;

	gint index;


	gtk_container_child_get (
		GTK_CONTAINER (priv->hdr_box), GTK_WIDGET (entry),
		"position", & index, NULL);

	entry_text = gtk_entry_get_text (entry);

	this->headers [index] = gtk_label_new (entry_text);
	gtk_misc_set_alignment (GTK_MISC (this->headers [index]), 0.0, 0.5);
	tile_attribute_set_string (priv->hdr_attrs [index], entry_text);

	gtk_widget_destroy (GTK_WIDGET (entry));

	gtk_container_add_with_properties (
		GTK_CONTAINER (priv->hdr_box), this->headers [index],
		"position", index, NULL);

	gtk_widget_show (this->headers [index]);
}
