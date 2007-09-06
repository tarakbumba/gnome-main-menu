#include "system-tile.h"

#include <glib/gi18n.h>

#include "desktop-item-tile-model.h"
#include "tile-button-view.h"
#include "context-menu-view.h"
#include "tile-attribute.h"
#include "tile-control.h"
#include "libslab-utils.h"
#include "bookmark-agent.h"

typedef struct {
	DesktopItemTileModel *model;
	TileButtonView       *view;

	TileControl *uri_control;
	TileControl *icon_control;
	TileControl *name_control;

	TileControl *open_menu_item_control;
	TileControl *is_in_store_control;
	TileControl *store_status_control;
} SystemTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SYSTEM_TILE_TYPE, SystemTilePrivate))

static void this_class_init (SystemTileClass *);
static void this_init       (SystemTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void clicked_cb                  (GtkButton *, gpointer);
static void open_item_activate_cb       (GtkMenuItem *, gpointer);
static void user_store_item_activate_cb (GtkMenuItem *, gpointer);

static void open_trigger         (TileAttribute *, TileAttribute *, gpointer);
static void is_in_store_trigger  (TileAttribute *, TileAttribute *, gpointer);
static void store_status_trigger (TileAttribute *, TileAttribute *, gpointer);

static TileClass *this_parent_class = NULL;

GType
system_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "SystemTile",
			sizeof (SystemTileClass), (GClassInitFunc) this_class_init,
			sizeof (SystemTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

SystemTile *
system_tile_new (const gchar *desktop_item_id, const gchar *title)
{
	SystemTile        *this;
	SystemTilePrivate *priv;

	ContextMenuView *menu;

	GtkWidget     *menu_item;
	TileAttribute *menu_attr;


	this = g_object_new (SYSTEM_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = desktop_item_tile_model_new (desktop_item_id);
	g_object_set (
		G_OBJECT (priv->model),
		DESKTOP_ITEM_TILE_MODEL_STORE_TYPE_PROP, BOOKMARK_STORE_SYSTEM,
		NULL);

	priv->view = tile_button_view_new (1);
	g_object_ref (G_OBJECT (priv->view));
	g_object_set (
		G_OBJECT (priv->view->icon), "icon-size", GTK_ICON_SIZE_MENU, NULL);

	menu = context_menu_view_new ();
	g_object_ref (G_OBJECT (menu));

	tile_button_view_add_context_menu (priv->view, GTK_MENU (menu));

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);

	priv->uri_control = tile_control_new (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_uri_attr (priv->view));
	priv->icon_control = tile_control_new (
		desktop_item_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view));

	if (title)
		tile_attribute_set_string (
			tile_button_view_get_header_text_attr (priv->view, 0), title);
	else
		priv->name_control = tile_control_new (
			desktop_item_tile_model_get_name_attr (priv->model),
			tile_button_view_get_header_text_attr (priv->view, 0));

/* make open app menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_item_activate_cb), this);

	priv->open_menu_item_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_name_attr (priv->model), menu_attr,
		open_trigger, NULL);

/* insert separator */

	gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());

/* make add/remove from favorites menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (user_store_item_activate_cb), this);

	priv->is_in_store_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_is_in_store_attr (priv->model), menu_attr,
		is_in_store_trigger, NULL);
	priv->store_status_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_store_status_attr (priv->model), menu_attr,
		store_status_trigger, NULL);

	gtk_widget_show_all (GTK_WIDGET (menu));

	return this;
}

static void
this_class_init (SystemTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (SystemTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	priv->model                  = NULL;
	priv->view                   = NULL;

	priv->uri_control            = NULL;
	priv->icon_control           = NULL;
	priv->name_control           = NULL;

	priv->open_menu_item_control = NULL;
	priv->is_in_store_control    = NULL;
	priv->store_status_control   = NULL;
}

static void
finalize (GObject *g_obj)
{
	SystemTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	g_object_unref (priv->uri_control);
	g_object_unref (priv->icon_control);

	if (G_IS_OBJECT (priv->name_control))
		g_object_unref (priv->name_control);

	g_object_unref (priv->open_menu_item_control);
	g_object_unref (priv->is_in_store_control);
	g_object_unref (priv->store_status_control);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static GtkWidget *
get_widget (Tile *this)
{
	return GTK_WIDGET (PRIVATE (this)->view);
}

static gboolean
equals (Tile *this, gconstpointer that)
{
	const gchar *uri_this;
	const gchar *uri_that;

	uri_this = tile_model_get_uri (TILE_MODEL (PRIVATE (this)->model));

	if (IS_SYSTEM_TILE (that))
		uri_that = tile_model_get_uri (TILE_MODEL (PRIVATE (that)->model));
	else
		uri_that = (const gchar *) that;

	return libslab_strcmp (uri_this, uri_that) == 0;
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	desktop_item_tile_model_start (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
open_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_start (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
user_store_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_user_store_toggle (PRIVATE (data)->model);
}

static void
open_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	gchar *menu_label;

	menu_label = g_strdup_printf (
		_("<b>Open %s</b>"), g_value_get_string (tile_attribute_get_value (src)));
	tile_attribute_set_string (dst, menu_label);
	g_free (menu_label);
}

static void
is_in_store_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_string (dst, _("Remove from System Items"));
	else
		tile_attribute_set_string (dst, _("Add to System Items"));
}

static void
store_status_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	switch (g_value_get_int (tile_attribute_get_value (src))) {
		case BOOKMARK_STORE_DEFAULT_ONLY:
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_INACTIVE);
			break;

		case BOOKMARK_STORE_ABSENT:
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_HIDDEN);
			break;

		default:
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
			break;
	}
}
