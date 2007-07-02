#include "context-menu-view.h"

#include <string.h>
#include <stdlib.h>

#include "tile-view.h"

typedef struct {
	GList *menu_item_attrs;

	gint item_index;
} ContextMenuViewPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CONTEXT_MENU_VIEW_TYPE, ContextMenuViewPrivate))

static void this_class_init (ContextMenuViewClass *);
static void this_init       (ContextMenuView *);

static void tile_view_interface_init (TileViewInterface *, gpointer);

static void finalize (GObject *);

static TileAttribute *get_attribute_by_id (TileView *, const gchar *);

static void menu_item_attr_value_notify_cb  (GObject *, GParamSpec *, gpointer);
static void menu_item_attr_status_notify_cb (GObject *, GParamSpec *, gpointer);

enum {
	PROP_0,
	PROP_DEBOUNCE
};

static GtkButtonClass *this_parent_class;

GType
context_menu_view_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0)) {
		static const GInterfaceInfo interface_info = {
			(GInterfaceInitFunc) tile_view_interface_init, NULL, NULL
		};

		type_id = g_type_register_static_simple (
			GTK_TYPE_MENU, "ContextMenuView",
			sizeof (ContextMenuViewClass), (GClassInitFunc) this_class_init,
			sizeof (ContextMenuView), (GInstanceInitFunc) this_init, 0);

		g_type_add_interface_static (type_id, TILE_VIEW_TYPE, & interface_info);
	}

	return type_id;
}

ContextMenuView *
context_menu_view_new ()
{
	return g_object_new (CONTEXT_MENU_VIEW_TYPE, NULL);
}

TileAttribute *
context_menu_view_add_menu_item (ContextMenuView *this, GtkWidget *menu_item)
{
	ContextMenuViewPrivate *priv = PRIVATE (this);

	TileAttribute *attr;


	gtk_menu_append (GTK_MENU (this), menu_item);

	attr = tile_attribute_new (G_TYPE_STRING);

	g_signal_connect (
		G_OBJECT (attr), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (menu_item_attr_value_notify_cb), menu_item);

	g_signal_connect (
		G_OBJECT (attr), "notify::" TILE_ATTRIBUTE_STATUS_PROP,
		G_CALLBACK (menu_item_attr_status_notify_cb), menu_item);

	priv->menu_item_attrs = g_list_append (priv->menu_item_attrs, attr);
	++priv->item_index;

	return attr;
}

static void
this_class_init (ContextMenuViewClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (ContextMenuViewPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
tile_view_interface_init (TileViewInterface *iface, gpointer data)
{
	iface->get_attribute_by_id = get_attribute_by_id;
}

static void
this_init (ContextMenuView *this)
{
	ContextMenuViewPrivate *priv = PRIVATE (this);

	priv->menu_item_attrs = NULL;
	priv->item_index      = 0;
}

static void
finalize (GObject *g_obj)
{
	ContextMenuViewPrivate *priv = PRIVATE (g_obj);

	GList *node;


	for (node = priv->menu_item_attrs; node; node = node->next)
		g_object_unref (node->data);

	g_list_free (priv->menu_item_attrs);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static TileAttribute *
get_attribute_by_id (TileView *this, const gchar *id)
{
	GList *node;

	g_return_val_if_fail (id, NULL);

	if (g_str_has_prefix (id, CONTEXT_MENU_VIEW_MENU_ITEM_ATTR_PREFIX)) {
		node = g_list_nth (
			PRIVATE (this)->menu_item_attrs,
			atoi (& id [strlen (CONTEXT_MENU_VIEW_MENU_ITEM_ATTR_PREFIX)]));

		if (node)
			return TILE_ATTRIBUTE (node->data);
	}
	else
		g_return_val_if_reached (NULL);

	return NULL;
}

static void
menu_item_attr_value_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	GtkMenuItem  *menu_item;
	GtkWidget    *label;
	const GValue *value;


	value = tile_attribute_get_value (TILE_ATTRIBUTE (g_obj));

	if (! (value && G_VALUE_HOLDS (value, G_TYPE_STRING)))
		return;

	menu_item = GTK_MENU_ITEM (data);
	label = gtk_bin_get_child (GTK_BIN (menu_item));

	if (! label) {
		label = gtk_label_new (NULL);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (menu_item), label);
	}

	gtk_label_set_markup (GTK_LABEL (label), g_value_get_string (value));
}

static void
menu_item_attr_status_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	GtkWidget *menu_item = GTK_WIDGET (data);

	TileAttributeStatus status;


	g_object_get (g_obj, TILE_ATTRIBUTE_STATUS_PROP, & status, NULL);

	switch (status) {
		case TILE_ATTRIBUTE_ACTIVE:
			gtk_widget_show (menu_item);
			gtk_widget_set_sensitive (menu_item, TRUE);
			break;

		case TILE_ATTRIBUTE_INACTIVE:
			gtk_widget_show (menu_item);
			gtk_widget_set_sensitive (menu_item, FALSE);
			break;

		case TILE_ATTRIBUTE_HIDDEN:
			gtk_widget_hide (menu_item);
			gtk_widget_set_no_show_all (menu_item, TRUE);
			break;

		default:
			break;
	}
}
