#include "application-tile.h"

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
	TileControl *name_hdr_control;
	TileControl *desc_hdr_control;
	TileControl *tooltip_control;

	TileControl *start_menu_item_control;
	TileControl *help_menu_item_control;
	TileControl *is_in_store_control;
	TileControl *store_status_control;
	TileControl *autostart_control;
	TileControl *can_manage_package_control;
} ApplicationTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), APPLICATION_TILE_TYPE, ApplicationTilePrivate))

static void this_class_init (ApplicationTileClass *);
static void this_init       (ApplicationTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void clicked_cb                  (GtkButton *, gpointer);
static void start_item_activate_cb      (GtkMenuItem *, gpointer);
static void help_item_activate_cb       (GtkMenuItem *, gpointer);
static void user_store_item_activate_cb (GtkMenuItem *, gpointer);
static void autostart_item_activate_cb  (GtkMenuItem *, gpointer);
static void upgrade_item_activate_cb    (GtkMenuItem *, gpointer);
static void uninstall_item_activate_cb  (GtkMenuItem *, gpointer);

static void start_trigger              (TileAttribute *, TileAttribute *, gpointer);
static void help_trigger               (TileAttribute *, TileAttribute *, gpointer);
static void is_in_store_trigger        (TileAttribute *, TileAttribute *, gpointer);
static void store_status_trigger       (TileAttribute *, TileAttribute *, gpointer);
static void autostart_status_trigger   (TileAttribute *, TileAttribute *, gpointer);
static void can_manage_package_trigger (TileAttribute *, TileAttribute *, gpointer);

static TileClass *this_parent_class = NULL;

GType
application_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "ApplicationTile",
			sizeof (ApplicationTileClass), (GClassInitFunc) this_class_init,
			sizeof (ApplicationTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

ApplicationTile *
application_tile_new (const gchar *desktop_item_id)
{
	ApplicationTile        *this;
	ApplicationTilePrivate *priv;

	DesktopItemTileModel *model;

	ContextMenuView *menu;

	GtkWidget     *menu_item;
	TileAttribute *menu_attr;


	model = desktop_item_tile_model_new (desktop_item_id);

	if (! model)
		return NULL;

	this = g_object_new (APPLICATION_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = model;
	priv->view  = tile_button_view_new (2);

	menu = context_menu_view_new ();

	tile_button_view_add_context_menu (priv->view, GTK_MENU (menu));

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);

	priv->uri_control = tile_control_new (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_uri_attr (priv->view));
	priv->icon_control = tile_control_new (
		desktop_item_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view));
	priv->name_hdr_control = tile_control_new (
		desktop_item_tile_model_get_name_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 0));
	priv->desc_hdr_control = tile_control_new (
		desktop_item_tile_model_get_description_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 1));
	priv->tooltip_control = tile_control_new (
		desktop_item_tile_model_get_comment_attr (priv->model),
		tile_button_view_get_tooltip_attr (priv->view));

/* make start app menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (start_item_activate_cb), this);

	priv->start_menu_item_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_name_attr (priv->model), menu_attr,
		start_trigger, NULL);

/* insert separator */

	gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());

/* make help menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Help"));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (help_item_activate_cb), this);

	priv->help_menu_item_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_help_available_attr (priv->model), menu_attr,
		help_trigger, NULL);

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

/* make add/remove from autostart menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (autostart_item_activate_cb), this);

	priv->autostart_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_autostart_status_attr (priv->model), menu_attr,
		autostart_status_trigger, NULL);

/* make upgrade menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Upgrade"));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (upgrade_item_activate_cb), this);

	priv->can_manage_package_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_can_manage_package_attr (priv->model), menu_attr,
		can_manage_package_trigger, NULL);

/* make uninstall menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Uninstall"));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (uninstall_item_activate_cb), this);

	priv->can_manage_package_control = tile_control_new_with_trigger_func (
		desktop_item_tile_model_get_can_manage_package_attr (priv->model), menu_attr,
		can_manage_package_trigger, NULL);

	gtk_widget_show_all (GTK_WIDGET (menu));

	return this;
}

static void
this_class_init (ApplicationTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (ApplicationTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (ApplicationTile *this)
{
	ApplicationTilePrivate *priv = PRIVATE (this);

	priv->model                      = NULL;
	priv->view                       = NULL;

	priv->uri_control                = NULL;
	priv->icon_control               = NULL;
	priv->name_hdr_control           = NULL;
	priv->desc_hdr_control           = NULL;
	priv->tooltip_control            = NULL;

	priv->start_menu_item_control    = NULL;
	priv->help_menu_item_control     = NULL;
	priv->is_in_store_control        = NULL;
	priv->store_status_control       = NULL;
	priv->autostart_control          = NULL;
	priv->can_manage_package_control = NULL;
}

static void
finalize (GObject *g_obj)
{
	ApplicationTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	g_object_unref (priv->uri_control);
	g_object_unref (priv->icon_control);
	g_object_unref (priv->name_hdr_control);
	g_object_unref (priv->desc_hdr_control);
	g_object_unref (priv->tooltip_control);

	g_object_unref (priv->start_menu_item_control);
	g_object_unref (priv->help_menu_item_control);
	g_object_unref (priv->is_in_store_control);
	g_object_unref (priv->store_status_control);
	g_object_unref (priv->autostart_control);
	g_object_unref (priv->can_manage_package_control);

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

	if (IS_APPLICATION_TILE (that))
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
start_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_start (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
help_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_open_help (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
user_store_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_user_store_toggle (PRIVATE (data)->model);
}

static void
autostart_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_autostart_toggle (PRIVATE (data)->model);
}

static void
upgrade_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_upgrade_package (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
uninstall_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	desktop_item_tile_model_uninstall_package (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
start_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	gchar *menu_label;

	menu_label = g_strdup_printf (
		_("<b>Start %s</b>"), g_value_get_string (tile_attribute_get_value (src)));
	tile_attribute_set_string (dst, menu_label);
	g_free (menu_label);
}

static void
help_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
	else
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_INACTIVE);
}

static void
is_in_store_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_string (dst, _("Remove from Favorites"));
	else
		tile_attribute_set_string (dst, _("Add to Favorites"));
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

static void
autostart_status_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	switch (g_value_get_int (tile_attribute_get_value (src))) {
		case APP_NOT_IN_AUTOSTART_DIR:
			tile_attribute_set_string (dst, _("Add to Startup Programs"));
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
			break;

		case APP_IN_USER_AUTOSTART_DIR:
			tile_attribute_set_string (dst, _("Remove from Startup Programs"));
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
			break;

		case APP_IN_SYSTEM_AUTOSTART_DIR:
			tile_attribute_set_string (dst, _("Remove from Startup Programs"));
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_INACTIVE);
			break;

		case APP_NOT_ELIGIBLE:
			tile_attribute_set_string (dst, _("Add from Startup Programs"));
			tile_attribute_set_status (dst, TILE_ATTRIBUTE_INACTIVE);
			break;

		default:
			break;
	}
}

static void
can_manage_package_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
	else
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_HIDDEN);
}
