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

	priv->view  = tile_button_view_new (1);

	menu = context_menu_view_new ();

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

#ifdef DOOBIE_DOO
/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006, 2007 Novell, Inc.
 *
 * The Main Menu is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Main Menu is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the Main Menu; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "system-tile.h"

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gmacros.h>
#include <gconf/gconf-client.h>
#include <libgnomeui/gnome-client.h>

#include "bookmark-agent.h"
#include "slab-gnome-util.h"
#include "libslab-utils.h"

G_DEFINE_TYPE (SystemTile, system_tile, NAMEPLATE_TILE_TYPE)

static void system_tile_finalize (GObject *);
static void system_tile_style_set (GtkWidget *, GtkStyle *);

static void load_image (SystemTile *);
static GtkWidget *create_header (const gchar *);

static void open_trigger   (Tile *, TileEvent *, TileAction *);
static void remove_trigger (Tile *, TileEvent *, TileAction *);

static void update_user_list_menu_item (SystemTile *);
static void agent_notify_cb (GObject *, GParamSpec *, gpointer);

typedef struct {
	GnomeDesktopItem *desktop_item;

	BookmarkAgent       *agent;
	BookmarkStoreStatus  agent_status;
	gulong               notify_signal_id;
	
	gchar    *image_id;
	gboolean  image_is_broken;
} SystemTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SYSTEM_TILE_TYPE, SystemTilePrivate))

GtkWidget *
system_tile_new (const gchar *desktop_item_id, const gchar *title)
{
	SystemTile        *this;
	SystemTilePrivate *priv;

	gchar     *uri    = NULL;
	GtkWidget *header = NULL;

	GtkMenu *context_menu;

	TileAction  **actions;
	TileAction   *action;
	GtkWidget    *menu_item;
	GtkContainer *menu_ctnr;

	GnomeDesktopItem *desktop_item = NULL;
	gchar            *image_id     = NULL;
	gchar            *header_txt   = NULL;

	gchar *markup;

	AtkObject *accessible = NULL;


	desktop_item = libslab_gnome_desktop_item_new_from_unknown_id (desktop_item_id);

	if (desktop_item) {
		image_id = g_strdup (gnome_desktop_item_get_localestring (desktop_item, "Icon"));
		uri      = g_strdup (gnome_desktop_item_get_location (desktop_item));

		if (title)
			header_txt = g_strdup (title);
		else
			header_txt = g_strdup (
				gnome_desktop_item_get_localestring (desktop_item, "Name"));
	}

	if (! uri)
		return NULL;

	header = create_header (header_txt);

	context_menu = GTK_MENU (gtk_menu_new ());

	this = g_object_new (
		SYSTEM_TILE_TYPE,
		"tile-uri",            uri,
		"context-menu",        context_menu,
		"nameplate-image",     gtk_image_new (),
		"nameplate-header",    header,
		"nameplate-subheader", NULL,
		NULL);
	priv = PRIVATE (this);

	priv->agent = bookmark_agent_get_instance (BOOKMARK_STORE_SYSTEM);
	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_STORE_STATUS_PROP, & priv->agent_status, NULL);

	priv->notify_signal_id = g_signal_connect (
		G_OBJECT (priv->agent), "notify", G_CALLBACK (agent_notify_cb), this);

	actions = g_new0 (TileAction *, 2);

	TILE (this)->actions   = actions;
	TILE (this)->n_actions = 2;

	menu_ctnr = GTK_CONTAINER (TILE (this)->context_menu);

	markup = g_markup_printf_escaped (_("<b>Open %s</b>"), header_txt);
	action = tile_action_new (TILE (this), open_trigger, markup, TILE_ACTION_OPENS_NEW_WINDOW);
	actions [SYSTEM_TILE_ACTION_OPEN] = action;
	g_free (markup);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	TILE (this)->default_action = action;

	gtk_container_add (menu_ctnr, gtk_separator_menu_item_new ());

	markup = g_markup_printf_escaped (_("Remove from System Items"));
	action = tile_action_new (TILE (this), remove_trigger, markup, 0);
	actions [SYSTEM_TILE_ACTION_REMOVE] = action;
	g_free (markup);

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));

	gtk_container_add (menu_ctnr, menu_item);

	gtk_widget_show_all (GTK_WIDGET (TILE (this)->context_menu));

	update_user_list_menu_item (this);

	priv->desktop_item = desktop_item;
	priv->image_id = g_strdup (image_id);

	load_image (this);

	/* Set up the mnemonic for the tile */
	gtk_label_set_mnemonic_widget (GTK_LABEL (header), GTK_WIDGET (this));

	/* Set up the accessible name for the tile */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (header_txt)
		atk_object_set_name (accessible, header_txt);

	g_free (header_txt);
	g_free (image_id);
	g_free (uri);

	return GTK_WIDGET (this);
}

static void
system_tile_class_init (SystemTileClass *this_class)
{
	GObjectClass   *g_obj_class  = G_OBJECT_CLASS   (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	g_obj_class->finalize = system_tile_finalize;

	widget_class->style_set = system_tile_style_set;

	g_type_class_add_private (this_class, sizeof (SystemTilePrivate));
}

static void
system_tile_init (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	priv->desktop_item    = NULL;
	priv->image_id        = NULL;
	priv->image_is_broken = TRUE;

	priv->agent            = NULL;
	priv->agent_status     = BOOKMARK_STORE_ABSENT;
	priv->notify_signal_id = 0;
}

static void
system_tile_finalize (GObject *g_obj)
{
        SystemTilePrivate *priv = PRIVATE (g_obj);

	g_free (priv->image_id);
	gnome_desktop_item_unref (priv->desktop_item);

	if (priv->notify_signal_id)
		g_signal_handler_disconnect (priv->agent, priv->notify_signal_id);

	G_OBJECT_CLASS (system_tile_parent_class)->finalize (g_obj);
}

static void
system_tile_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
	load_image (SYSTEM_TILE (widget));
}

static void
load_image (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	GtkImage *image = GTK_IMAGE (NAMEPLATE_TILE (this)->image);


	g_object_set (G_OBJECT (image), "icon-size", GTK_ICON_SIZE_MENU, NULL);

	priv->image_is_broken = libslab_gtk_image_set_by_id (image, priv->image_id);
}

static GtkWidget *
create_header (const gchar *name)
{
	GtkWidget *header;

	header = gtk_label_new (name);
	gtk_label_set_use_underline (GTK_LABEL (header), TRUE);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	return header;
}

static void
open_trigger (Tile *this, TileEvent *event, TileAction *action)
{
	open_desktop_item_exec (PRIVATE (this)->desktop_item);
}

static void
remove_trigger (Tile *this, TileEvent *event, TileAction *action)
{
	bookmark_agent_remove_item (PRIVATE (this)->agent, this->uri);
}

static void
update_user_list_menu_item (SystemTile *this)
{
	SystemTilePrivate *priv = PRIVATE (this);

	TileAction *action;
	GtkWidget  *item;


	action = TILE (this)->actions [SYSTEM_TILE_ACTION_REMOVE];

	if (! action)
		return;

	item = GTK_WIDGET (tile_action_get_menu_item (action));

	if (! GTK_IS_MENU_ITEM (item))
		return;

	g_object_get (G_OBJECT (priv->agent), BOOKMARK_AGENT_STORE_STATUS_PROP, & priv->agent_status, NULL);

	gtk_widget_set_sensitive (item, (priv->agent_status != BOOKMARK_STORE_DEFAULT_ONLY));
}

static void
agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	update_user_list_menu_item (SYSTEM_TILE (user_data));
}
#endif
