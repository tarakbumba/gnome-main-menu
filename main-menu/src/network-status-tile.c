/*
 * This file is part of the Main Menu.
 *
 * Copyright (c) 2006 Novell, Inc.
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

#include "network-status-tile.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "network-status-agent.h"
#include "network-status-info.h"

#define GNOME_MAIN_MENU_SCHEMA "org.mate.gnome-main-menu"
#define NETWORK_CONFIG_TOOL_KEY "network-config-tool"
#define NETWORK_CONFIG_TOOL_NM_KEY "network-config-tool-nm"

G_DEFINE_TYPE (NetworkStatusTile, network_status_tile, NAMEPLATE_TILE_TYPE)

static void network_status_tile_finalize (GObject *);
static void network_status_tile_style_set (GtkWidget *, GtkStyle *);

static void network_status_tile_activated (Tile *, TileEvent *);

static void network_status_tile_open (Tile *, TileEvent *, TileAction *);

static void update_tile (NetworkStatusTile *);
static void refresh_status (NetworkStatusTile *);

static void tile_show_event_cb (GtkWidget *, gpointer);
static void status_changed_cb (NetworkStatusAgent *, gpointer);

static void set_ui_label (GtkBuilder *, const gchar *, const gchar *);
static void launch_network_config (const gchar * desktop_key);

typedef struct
{
	NetworkStatusAgent *agent;
	NetworkStatusInfo *status_info;
	
	GtkBuilder *info_dialog_ui;
	GtkWidget *info_dialog;
} NetworkStatusTilePrivate;

#define NETWORK_STATUS_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NETWORK_STATUS_TILE_TYPE, NetworkStatusTilePrivate))

static void network_status_tile_class_init (NetworkStatusTileClass * net_tile_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (net_tile_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (net_tile_class);
	TileClass *tile_class = TILE_CLASS (net_tile_class);

	g_obj_class->finalize = network_status_tile_finalize;

	widget_class->style_set = network_status_tile_style_set;

	tile_class->tile_activated = network_status_tile_activated;

	g_type_class_add_private (net_tile_class, sizeof (NetworkStatusTilePrivate));
}

GtkWidget *
network_status_tile_new ()
{
	NetworkStatusTile *tile;

	GtkWidget *image;
	GtkWidget *header;
	GtkWidget *subheader;

	image = gtk_image_new ();
	slab_load_image (GTK_IMAGE (image), GTK_ICON_SIZE_BUTTON, "nm-no-connection");

	header = gtk_label_new (_("Network: None"));
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	subheader = gtk_label_new (_("Click to configure network"));
	gtk_label_set_ellipsize (GTK_LABEL (subheader), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (subheader), 0.0, 0.5);
	gtk_widget_modify_fg (subheader, GTK_STATE_NORMAL,
		&subheader->style->fg[GTK_STATE_INSENSITIVE]);

	tile = g_object_new (NETWORK_STATUS_TILE_TYPE, "tile-uri", "tile://network-status",
		"nameplate-image", image, "nameplate-header", header, "nameplate-subheader",
		subheader, NULL);

	TILE (tile)->actions = g_new0 (TileAction *, 1);
	TILE (tile)->n_actions = 1;

	TILE (tile)->actions[NETWORK_STATUS_TILE_ACTION_OPEN] =
		tile_action_new (TILE (tile), network_status_tile_open, NULL,
		TILE_ACTION_OPENS_NEW_WINDOW);

	TILE (tile)->default_action = TILE (tile)->actions[NETWORK_STATUS_TILE_ACTION_OPEN];

	g_signal_connect (G_OBJECT (tile), "show", G_CALLBACK (tile_show_event_cb), NULL);

	return GTK_WIDGET (tile);
}

static void
network_status_tile_init (NetworkStatusTile * tile)
{
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);

	priv->agent = NULL;
	priv->info_dialog_ui = NULL;
	priv->info_dialog = NULL;
}

static void
network_status_tile_finalize (GObject * g_object)
{
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (g_object);
	if (priv->status_info)
		g_object_unref (priv->status_info);
	if (priv->agent)
		g_object_unref (priv->agent);
	if (priv->info_dialog_ui)
		g_object_unref (priv->info_dialog_ui);

	(*G_OBJECT_CLASS (network_status_tile_parent_class)->finalize) (g_object);
}

static void
network_status_tile_style_set (GtkWidget * widget, GtkStyle * prev_style)
{
	update_tile (NETWORK_STATUS_TILE (widget));
}

static void
network_status_tile_activated (Tile * tile, TileEvent * event)
{
	if (event->type != TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static void
network_status_tile_open (Tile * tile, TileEvent * event, TileAction * action)
{
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);

	if (!priv->status_info || !priv->agent->nm_present)
	{
		launch_network_config (NETWORK_CONFIG_TOOL_KEY);

		return;
	}
	else
	{
		launch_network_config (NETWORK_CONFIG_TOOL_NM_KEY);
		return;
	}
}

static void
update_tile (NetworkStatusTile * tile)
{
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);
	AtkObject *accessible;

	gchar *icon_name;
	gchar *header_text;
	gchar *subheader_text;

	gchar *markup = NULL;

	if (!priv->status_info)
	{
		icon_name = "nm-no-connection";
		header_text = _("Networ_k: None");
		subheader_text = _("Click to configure network");
	}

	else
	{
		switch (priv->status_info->type)
		{
		case NM_DEVICE_TYPE_WIFI:
			markup = g_strdup_printf (_("Connected to: %s"), priv->status_info->essid);

			icon_name = "nm-device-wireless";
			header_text = _("Networ_k: Wireless");
			subheader_text = markup;
			break;

		case NM_DEVICE_TYPE_ETHERNET:
			markup = g_strdup_printf (_("Using ethernet (%s)"),
				priv->status_info->iface);

			icon_name = "nm-device-wired";
			header_text = _("Networ_k: Wired");
			subheader_text = markup;
			break;

		case NM_DEVICE_TYPE_MODEM:
			markup = g_strdup_printf (_("Connected to: %s"), priv->status_info->iface);

			icon_name = "nm-device-wireless";
			header_text = _("Networ_k: GSM");
			subheader_text = markup;
			break;

		default:
			icon_name = "";
			header_text = "";
			subheader_text = "";
			break;
		}
	}

	slab_load_image (GTK_IMAGE (NAMEPLATE_TILE (tile)->image), GTK_ICON_SIZE_BUTTON, icon_name);
	gtk_label_set_text (GTK_LABEL (NAMEPLATE_TILE (tile)->header), header_text);
	gtk_label_set_text (GTK_LABEL (NAMEPLATE_TILE (tile)->subheader), subheader_text);

	gtk_label_set_use_underline (GTK_LABEL (NAMEPLATE_TILE (tile)->header), TRUE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (NAMEPLATE_TILE (tile)->header), GTK_WIDGET (tile));

	accessible = gtk_widget_get_accessible (GTK_WIDGET (tile));
	if (header_text)
	  atk_object_set_name (accessible, header_text);
	if (subheader_text)
	  atk_object_set_description (accessible, subheader_text);

	g_free (markup);
}

static void
refresh_status (NetworkStatusTile * tile)
{
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);

	if (!priv->agent)
	{
		priv->agent = network_status_agent_new ();

		g_signal_connect (G_OBJECT (priv->agent), "status-changed",
			G_CALLBACK (status_changed_cb), tile);
	}

	if (priv->agent)
	{
		if (priv->status_info)
			g_object_unref (priv->status_info);

		priv->status_info = network_status_agent_get_first_active_device_info (priv->agent);
	}
}

static void
tile_show_event_cb (GtkWidget * widget, gpointer user_data)
{
	NetworkStatusTile *tile = NETWORK_STATUS_TILE (widget);
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);

	if (!priv->status_info)
		refresh_status (tile);

	update_tile (tile);
}

void
network_tile_update_status (GtkWidget * widget)
{
	NetworkStatusTile *tile = NETWORK_STATUS_TILE (widget);
	NetworkStatusTilePrivate *priv = NETWORK_STATUS_TILE_GET_PRIVATE (tile);

	if (priv->agent->nm_present)
		return;

	refresh_status (tile);
	update_tile (tile);
}

static void
status_changed_cb (NetworkStatusAgent * agent, gpointer user_data)
{
	refresh_status (NETWORK_STATUS_TILE (user_data));
	update_tile (NETWORK_STATUS_TILE (user_data));
}

static void
set_ui_label (GtkBuilder * ui, const gchar * id, const gchar * text)
{
	GtkLabel *label = NULL;

	label = GTK_LABEL (gtk_builder_get_object (ui, id));

	if (label)
		gtk_label_set_text (label, text);
}

static void
launch_network_config (const gchar * desktop_key)
{
	GSettings *settings;
	GError *error = NULL;
	GDesktopAppInfo *appinfo;
	gchar *desktop_file;

	settings = g_settings_new (GNOME_MAIN_MENU_SCHEMA);
	desktop_file = g_settings_get_string (settings, desktop_key);
	g_object_unref (settings);
	appinfo = g_desktop_app_info_new (desktop_file);

	if (appinfo)
	{
		g_app_info_launch (G_APP_INFO (appinfo), NULL, NULL, &error);
		if (error) {
			g_warning ("network_status_tile_open: couldn't exec item: %s\n", error->message);
			g_error_free (error);
		}
		g_object_unref (appinfo);
	}
	else
	{
		g_warning ("network_status_tile_open: couldn't exec item: %s\n", desktop_file);
	}
	g_free (desktop_file);
}
