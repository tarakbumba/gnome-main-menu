#include "directory-tile.h"

#include <glib/gi18n.h>

#include "directory-tile-model.h"
#include "tile-button-view.h"
#include "context-menu-view.h"
#include "tile-attribute.h"
#include "tile-control.h"
#include "libslab-utils.h"

typedef struct {
	DirectoryTileModel *model;
	TileButtonView     *view;

	TileControl *uri_control;
	TileControl *icon_control;
	TileControl *name_control;

	TileControl *send_to_menu_item_control;
	TileControl *can_delete_control;
} DirectoryTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DIRECTORY_TILE_TYPE, DirectoryTilePrivate))

static void this_class_init (DirectoryTileClass *);
static void this_init       (DirectoryTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void clicked_cb               (GtkButton *, gpointer);
static void open_item_activate_cb    (GtkMenuItem *, gpointer);
static void send_to_item_activate_cb (GtkMenuItem *, gpointer);
static void trash_item_activate_cb   (GtkMenuItem *, gpointer);
static void delete_item_activate_cb  (GtkMenuItem *, gpointer);

static void send_to_trigger    (TileAttribute *, TileAttribute *, gpointer);
static void can_delete_trigger (TileAttribute *, TileAttribute *, gpointer);

static TileClass *this_parent_class = NULL;

GType
directory_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "DirectoryTile",
			sizeof (DirectoryTileClass), (GClassInitFunc) this_class_init,
			sizeof (DirectoryTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

DirectoryTile *
directory_tile_new (const gchar *uri)
{
	DirectoryTile        *this;
	DirectoryTilePrivate *priv;

	ContextMenuView *menu;

	GtkWidget     *menu_item;
	GtkWidget     *menu_item_child;
	TileAttribute *menu_attr;


	this = g_object_new (DIRECTORY_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = directory_tile_model_new (uri);
	priv->view = tile_button_view_new (1);

	menu = context_menu_view_new ();

	tile_button_view_add_context_menu (priv->view, GTK_MENU (menu));

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);

	priv->uri_control = tile_control_new (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_uri_attr (priv->view));
	priv->icon_control = tile_control_new (
		directory_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view));
	priv->name_control = tile_control_new (
		directory_tile_model_get_name_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 0));

	priv->can_delete_control = tile_control_new_with_trigger_func (
		directory_tile_model_get_can_delete_attr (priv->model), menu_attr,
		can_delete_trigger, NULL);

/* make open app menu-item */

	menu_item = gtk_menu_item_new_with_label (_("<b>Open</b>"));
	menu_item_child = gtk_bin_get_child (GTK_BIN (menu_item));
	gtk_label_set_use_markup (GTK_LABEL (menu_item_child), TRUE);
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_item_activate_cb), this);

/* insert separator */

	gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());

/* make send-to menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Send To..."));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (send_to_item_activate_cb), this);

	priv->send_to_menu_item_control = tile_control_new_with_trigger_func (
		directory_tile_model_get_is_local_attr (priv->model), menu_attr,
		send_to_trigger, NULL);

/* insert separator */

	gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());

/* make trash menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Move to Trash"));
	gtk_menu_append (GTK_MENU (menu), menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (trash_item_activate_cb), this);

/* make delete menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Delete"));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (delete_item_activate_cb), this);

	priv->can_delete_control = tile_control_new_with_trigger_func (
		directory_tile_model_get_can_delete_attr (priv->model), menu_attr,
		can_delete_trigger, NULL);

	gtk_widget_show_all (GTK_WIDGET (menu));

	return this;
}

static void
this_class_init (DirectoryTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (DirectoryTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (DirectoryTile *this)
{
	DirectoryTilePrivate *priv = PRIVATE (this);

	priv->model                  = NULL;
	priv->view                   = NULL;

	priv->uri_control            = NULL;
	priv->icon_control           = NULL;
	priv->name_control           = NULL;

	priv->can_delete_control     = NULL;
}

static void
finalize (GObject *g_obj)
{
	DirectoryTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	g_object_unref (priv->uri_control);
	g_object_unref (priv->icon_control);
	g_object_unref (priv->name_control);

	g_object_unref (priv->can_delete_control);

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

	if (IS_DIRECTORY_TILE (that))
		uri_that = tile_model_get_uri (TILE_MODEL (PRIVATE (that)->model));
	else
		uri_that = (const gchar *) that;

	return libslab_strcmp (uri_this, uri_that) == 0;
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	directory_tile_model_open (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
open_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	directory_tile_model_open (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
send_to_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	directory_tile_model_send_to (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
trash_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	directory_tile_model_trash (PRIVATE (data)->model);
}

static void
delete_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	directory_tile_model_delete (PRIVATE (data)->model);
}

static void
send_to_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
	else
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_INACTIVE);
}

static void
can_delete_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
	else
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_HIDDEN);
}
