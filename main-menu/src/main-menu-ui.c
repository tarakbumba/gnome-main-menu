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

#include "main-menu-ui.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <panel-applet.h>
#include <glade/glade.h>
#include <cairo.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gdk/gdkkeysyms.h>

#include "tile.h"
#include "hard-drive-status-tile.h"
#include "network-status-tile.h"

#include "user-apps-tile-table.h"
#include "recent-apps-tile-table.h"
#include "user-docs-tile-table.h"
#include "recent-docs-tile-table.h"
#include "user-dirs-tile-table.h"
#include "system-tile-table.h"

#include "tomboykeybinder.h"
#include "libslab-utils.h"
#include "double-click-detector.h"

#define CURRENT_PAGE_GCONF_KEY     "/desktop/gnome/applications/main-menu/file-area/file_class"
#define URGENT_CLOSE_GCONF_KEY     "/desktop/gnome/applications/main-menu/urgent_close"
#define MAX_TOTAL_ITEMS_GCONF_KEY  "/desktop/gnome/applications/main-menu/file-area/max_total_items"
#define MIN_RECENT_ITEMS_GCONF_KEY "/desktop/gnome/applications/main-menu/file-area/min_recent_items"
#define APP_BROWSER_GCONF_KEY      "/desktop/gnome/applications/main-menu/application_browser"
#define FILE_BROWSER_GCONF_KEY     "/desktop/gnome/applications/main-menu/file_browser"
#define SEARCH_CMD_GCONF_KEY       "/desktop/gnome/applications/main-menu/search_command"

G_DEFINE_TYPE (MainMenuUI, main_menu_ui, G_TYPE_OBJECT)

typedef struct {
	PanelApplet *panel_applet;
	GtkWidget   *panel_about_dialog;

	GladeXML *main_menu_xml;
	GladeXML *panel_button_xml;

	GtkWidget *panel_buttons [4];
	GtkWidget *panel_button;

	GtkWidget *slab_window;

	GtkWidget *top_pane;
	GtkWidget *left_pane;

	GtkWidget *search_section;
	GtkWidget *search_entry;

	GtkNotebook     *file_section;
	GtkToggleButton *apps_selector;
	GtkToggleButton *docs_selector;
	GtkToggleButton *dirs_selector;

	TileTable *sys_table;
	TileTable *usr_apps_table;
	TileTable *rct_apps_table;
	TileTable *usr_docs_table;
	TileTable *rct_docs_table;
	TileTable *usr_dirs_table;

	GtkWidget *more_button [3];

	gint max_total_items;

	guint    search_cmd_gconf_mntr_id;
	gboolean ptr_is_grabbed;
	gboolean kbd_is_grabbed;
} MainMenuUIPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_UI_TYPE, MainMenuUIPrivate))

static void main_menu_ui_finalize (GObject *);

static void create_panel_button      (MainMenuUI *);
static void create_slab_window       (MainMenuUI *);
static void create_search_section    (MainMenuUI *);
static void create_file_section      (MainMenuUI *);
static void create_user_apps_section (MainMenuUI *);
static void create_rct_apps_section  (MainMenuUI *);
static void create_user_docs_section (MainMenuUI *);
static void create_rct_docs_section  (MainMenuUI *);
static void create_user_dirs_section (MainMenuUI *);
static void create_system_section    (MainMenuUI *);
static void create_status_section    (MainMenuUI *);
static void create_more_buttons      (MainMenuUI *);

static void    set_panel_button_active    (MainMenuUI *, gboolean);
static void    select_page                (MainMenuUI *, gint);
static void    update_limits              (MainMenuUI *);
static void    connect_to_tile_triggers   (MainMenuUI *, TileTable *);
static void    hide_slab_if_urgent_close  (MainMenuUI *);
static void    set_search_section_visible (MainMenuUI *this);
static gchar **get_search_argv            (const gchar *);
static void    reorient_panel_button      (MainMenuUI *);
static void    bind_beagle_search_key     (MainMenuUI *);
static void    launch_search              (MainMenuUI *);
static void    grab_pointer_and_keyboard  (MainMenuUI *, guint32);

static gboolean panel_button_button_press_cb      (GtkWidget *, GdkEventButton *, gpointer);
static void     panel_button_drag_data_rcv_cb     (GtkWidget *, GdkDragContext *, gint, gint,
                                                   GtkSelectionData *, guint, guint, gpointer);
static gboolean slab_window_expose_cb             (GtkWidget *, GdkEventExpose *, gpointer);
static gboolean slab_window_key_press_cb          (GtkWidget *, GdkEventKey *, gpointer);
static gboolean slab_window_button_press_cb       (GtkWidget *, GdkEventButton *, gpointer);
static void     slab_window_allocate_cb           (GtkWidget *, GtkAllocation *, gpointer);
static void     slab_window_map_event_cb          (GtkWidget *, GdkEvent *, gpointer);
static void     slab_window_unmap_event_cb        (GtkWidget *, GdkEvent *, gpointer);
static gboolean slab_window_grab_broken_cb        (GtkWidget *, GdkEvent *, gpointer);
static void     search_entry_activate_cb          (GtkEntry *, gpointer);
static void     page_button_clicked_cb            (GtkButton *, gpointer);
static void     tile_table_notify_cb              (GObject *, GParamSpec *, gpointer);
static void     gtk_table_notify_cb               (GObject *, GParamSpec *, gpointer);
static void     tile_action_triggered_cb          (Tile *, TileEvent *, TileAction *, gpointer);
static void     more_button_clicked_cb            (GtkButton *, gpointer);
static void     search_cmd_notify_cb              (GConfClient *, guint, GConfEntry *, gpointer);
static void     panel_menu_open_cb                (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_menu_about_cb               (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_applet_change_orient_cb     (PanelApplet *, PanelAppletOrient, gpointer);
static void     panel_applet_change_background_cb (PanelApplet *, PanelAppletBackgroundType, GdkColor *,
                                                   GdkPixmap * pixmap, gpointer);
static void     slab_window_tomboy_bindkey_cb     (gchar *, gpointer);
static void     search_tomboy_bindkey_cb          (gchar *, gpointer);
static gboolean grabbing_window_event_cb          (GtkWidget *, GdkEvent *, gpointer);

static const BonoboUIVerb applet_bonobo_verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("MainMenuOpen",  panel_menu_open_cb),
	BONOBO_UI_UNSAFE_VERB ("MainMenuAbout", panel_menu_about_cb),
	BONOBO_UI_VERB_END
};

static const gchar *main_menu_authors [] = {
	"Jim Krehl <jimmyk@novell.com>",
	"Scott Reeves <sreeves@novell.com>",
	"Dan Winship <danw@novell.com>",
	NULL
};

static const gchar *main_menu_artists [] = {
	"Garrett LeSage <garrett@novell.com>",
	"Jakub Steiner <jimmac@novell.com>",
	NULL
};

enum {
	MORE_APPS_BUTTON,
	MORE_DOCS_BUTTON,
	MORE_DIRS_BUTTON
};

enum {
	PANEL_BUTTON_ORIENT_TOP,
	PANEL_BUTTON_ORIENT_BOTTOM,
	PANEL_BUTTON_ORIENT_LEFT,
	PANEL_BUTTON_ORIENT_RIGHT
};

MainMenuUI *
main_menu_ui_new (PanelApplet *applet)
{
	MainMenuUI        *this;
	MainMenuUIPrivate *priv;

	gchar *glade_xml_path;


	this = g_object_new (MAIN_MENU_UI_TYPE, NULL);
	priv = PRIVATE (this);

	priv->panel_applet = applet;

	glade_xml_path = g_build_filename (DATADIR, PACKAGE, "slab-window.glade", NULL);

	priv->main_menu_xml    = glade_xml_new (glade_xml_path, "slab-main-menu-window", NULL);
	priv->panel_button_xml = glade_xml_new (glade_xml_path, "slab-panel-button-root", NULL);

	create_panel_button      (this);
	create_slab_window       (this);
	create_search_section    (this);
	create_file_section      (this);
	create_user_apps_section (this);
	create_rct_apps_section  (this);
	create_user_docs_section (this);
	create_rct_docs_section  (this);
	create_user_dirs_section (this);
	create_system_section    (this);
	create_status_section    (this);
	create_more_buttons      (this);

	select_page   (this, -1);
	update_limits (this);

	return this;
}

static void
main_menu_ui_class_init (MainMenuUIClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	g_obj_class->finalize = main_menu_ui_finalize;

	g_type_class_add_private (this_class, sizeof (MainMenuUIPrivate));
}

static void
main_menu_ui_init (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->panel_applet                               = NULL;
	priv->panel_about_dialog                         = NULL;

	priv->main_menu_xml                              = NULL;
	priv->panel_button_xml                           = NULL;

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP]    = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM] = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT]   = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT]  = NULL;
	priv->panel_button                               = NULL;

	priv->slab_window                                = NULL;

	priv->top_pane                                   = NULL;
	priv->left_pane                                  = NULL;

	priv->search_section                             = NULL;
	priv->search_entry                               = NULL;

	priv->file_section                               = NULL;
	priv->apps_selector                              = NULL;
	priv->docs_selector                              = NULL;
	priv->dirs_selector                              = NULL;

	priv->sys_table                                  = NULL;
	priv->usr_apps_table                             = NULL;
	priv->rct_apps_table                             = NULL;
	priv->usr_docs_table                             = NULL;
	priv->rct_docs_table                             = NULL;
	priv->usr_dirs_table                             = NULL;

	priv->more_button [MORE_APPS_BUTTON]             = NULL;
	priv->more_button [MORE_DOCS_BUTTON]             = NULL;
	priv->more_button [MORE_DIRS_BUTTON]             = NULL;

	priv->max_total_items                            = 8;

	priv->search_cmd_gconf_mntr_id                   = 0;
	priv->ptr_is_grabbed                             = FALSE;
	priv->kbd_is_grabbed                             = FALSE;
}

static void
main_menu_ui_finalize (GObject *g_obj)
{
	MainMenuUIPrivate *priv = PRIVATE (g_obj);

	gint i;


	for (i = 0; i < 4; ++i) {
		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->more_button [i]), "double-click-detector")));

		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector")));

		gtk_widget_unref (priv->panel_buttons [i]);
	}

	libslab_gconf_notify_remove (priv->search_cmd_gconf_mntr_id);

	G_OBJECT_CLASS (main_menu_ui_parent_class)->finalize (g_obj);
}

static void
create_panel_button (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *button_root;
	GtkWidget *button_parent;

	GtkImage *image;

	gint i;


	button_root = glade_xml_get_widget (
		priv->panel_button_xml, "slab-panel-button-root");

	gtk_widget_hide (button_root);

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-top");
	priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-bottom");
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-left");
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-right");

	image = GTK_IMAGE (glade_xml_get_widget (panel_button_xml, "image44"));

	for (i = 0; i < 4; ++i) {
		g_object_set_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector",
			double_click_detector_new ());

		button_parent = gtk_widget_get_parent (priv->panel_buttons [i]);

		gtk_widget_ref (priv->panel_buttons [i]);
		gtk_container_remove (
			GTK_CONTAINER (button_parent), priv->panel_buttons [i]);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "button_press_event",
			G_CALLBACK (panel_button_button_press_cb), this);

		gtk_drag_dest_set (
			priv->panel_buttons [i],
			GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_dest_add_uri_targets (priv->panel_buttons [i]);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "drag-data-received",
			G_CALLBACK (panel_button_drag_data_rcv_cb), this);
	}

	gtk_widget_destroy (button_root);

	panel_applet_set_flags (priv->panel_applet, PANEL_APPLET_EXPAND_MINOR);

	reorient_panel_button (this);

	panel_applet_setup_menu_from_file (
		priv->panel_applet, NULL, "GNOME_MainMenu_ContextMenu.xml",
		NULL, applet_bonobo_verbs, this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_orient",
		G_CALLBACK (panel_applet_change_orient_cb), this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_background",
		G_CALLBACK (panel_applet_change_background_cb), this);
}

static void
create_slab_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->slab_window = glade_xml_get_widget (
		priv->main_menu_xml, "slab-main-menu-window");
	gtk_widget_set_app_paintable (priv->slab_window, TRUE);
	gtk_widget_hide              (priv->slab_window);
	gtk_window_stick             (GTK_WINDOW (priv->slab_window));

	priv->top_pane  = glade_xml_get_widget (priv->main_menu_xml, "top-pane");
	priv->left_pane = glade_xml_get_widget (priv->main_menu_xml, "left-pane");

	tomboy_keybinder_init ();
	tomboy_keybinder_bind ("<Ctrl>Escape", slab_window_tomboy_bindkey_cb, this);
	bind_beagle_search_key (this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "expose-event",
		G_CALLBACK (slab_window_expose_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "key-press-event",
		G_CALLBACK (slab_window_key_press_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "button-press-event",
		G_CALLBACK (slab_window_button_press_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "grab-broken-event",
		G_CALLBACK (slab_window_grab_broken_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "size-allocate",
		G_CALLBACK (slab_window_allocate_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "map-event",
		G_CALLBACK (slab_window_map_event_cb), this);

	g_signal_connect (
		G_OBJECT (priv->slab_window), "unmap-event",
		G_CALLBACK (slab_window_unmap_event_cb), this);
}

static void
create_search_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

        priv->search_section = glade_xml_get_widget (priv->main_menu_xml, "search-section");
	priv->search_entry   = glade_xml_get_widget (priv->main_menu_xml, "search-entry");

	g_signal_connect (
		G_OBJECT (priv->search_entry), "activate",
		G_CALLBACK (search_entry_activate_cb), this);

	set_search_section_visible (this);

	priv->search_cmd_gconf_mntr_id = libslab_gconf_notify_add (
		SEARCH_CMD_GCONF_KEY, search_cmd_notify_cb, this);
}

static void
create_file_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->file_section = GTK_NOTEBOOK (glade_xml_get_widget (
		priv->main_menu_xml, "file-area-notebook"));
	priv->apps_selector = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-applications"));
	priv->docs_selector = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-documents"));
	priv->dirs_selector = GTK_TOGGLE_BUTTON (glade_xml_get_widget (
		priv->main_menu_xml, "slab-page-selector-button-places"));

	g_signal_connect (
		G_OBJECT (priv->apps_selector), "clicked",
		G_CALLBACK (page_button_clicked_cb), this);

	g_signal_connect (
		G_OBJECT (priv->docs_selector), "clicked",
		G_CALLBACK (page_button_clicked_cb), this);

	g_signal_connect (
		G_OBJECT (priv->dirs_selector), "clicked",
		G_CALLBACK (page_button_clicked_cb), this);
}

static void
create_system_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "system-item-table-container"));

	priv->sys_table = TILE_TABLE (system_tile_table_new ());

	connect_to_tile_triggers (this, priv->sys_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->sys_table));

	g_signal_connect (
		G_OBJECT (priv->sys_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);
}

static void
create_status_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;
	GtkWidget    *tile;

	gint icon_width;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "hard-drive-status-container"));
	tile = hard_drive_status_tile_new ();

	gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);
	gtk_widget_set_size_request (tile, 6 * icon_width, -1);

	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));

	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "network-status-container"));
	tile = network_status_tile_new ();

	gtk_widget_set_size_request (tile, 6 * icon_width, -1);

	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));
}

static void
create_user_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-apps-table-container"));

	priv->usr_apps_table = TILE_TABLE (user_apps_tile_table_new ());
	gtk_table_set_row_spacings (GTK_TABLE (priv->usr_apps_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (priv->usr_apps_table), 6);

	connect_to_tile_triggers (this, priv->usr_apps_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->usr_apps_table));

	g_signal_connect (
		G_OBJECT (priv->usr_apps_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->usr_apps_table), "notify::n-rows",
		G_CALLBACK (gtk_table_notify_cb), this);
}

static void
create_rct_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "recent-apps-table-container"));

	priv->rct_apps_table = TILE_TABLE (recent_apps_tile_table_new ());
	gtk_table_set_row_spacings (GTK_TABLE (priv->rct_apps_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (priv->rct_apps_table), 6);

	connect_to_tile_triggers (this, priv->rct_apps_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->rct_apps_table));

	g_signal_connect (
		G_OBJECT (priv->rct_apps_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);
}

static void
create_user_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-docs-table-container"));

	priv->usr_docs_table = TILE_TABLE (user_docs_tile_table_new ());
	gtk_table_set_row_spacings (GTK_TABLE (priv->usr_docs_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (priv->usr_docs_table), 6);

	connect_to_tile_triggers (this, priv->usr_docs_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->usr_docs_table));

	g_signal_connect (
		G_OBJECT (priv->usr_docs_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->usr_docs_table), "notify::n-rows",
		G_CALLBACK (gtk_table_notify_cb), this);
}

static void
create_rct_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "recent-docs-table-container"));

	priv->rct_docs_table = TILE_TABLE (recent_docs_tile_table_new ());
	gtk_table_set_row_spacings (GTK_TABLE (priv->rct_docs_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (priv->rct_docs_table), 6);

	connect_to_tile_triggers (this, priv->rct_docs_table);

	g_object_set (G_OBJECT (priv->rct_docs_table), TILE_TABLE_LIMIT_PROP, 6, NULL);

	gtk_container_add (ctnr, GTK_WIDGET (priv->rct_docs_table));

	g_signal_connect (
		G_OBJECT (priv->rct_docs_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);
}

static void
create_user_dirs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "user-dirs-table-container"));

	priv->usr_dirs_table = TILE_TABLE (user_dirs_tile_table_new ());
	gtk_table_set_row_spacings (GTK_TABLE (priv->usr_dirs_table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (priv->usr_dirs_table), 6);

	connect_to_tile_triggers (this, priv->usr_dirs_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->usr_dirs_table));

	g_signal_connect (
		G_OBJECT (priv->usr_dirs_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->usr_dirs_table), "notify::n-rows",
		G_CALLBACK (gtk_table_notify_cb), this);
}

static void
create_more_buttons (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint i;


	priv->more_button [0] = glade_xml_get_widget (priv->main_menu_xml, "more-applications-button");
	priv->more_button [1] = glade_xml_get_widget (priv->main_menu_xml, "more-documents-button");
	priv->more_button [2] = glade_xml_get_widget (priv->main_menu_xml, "more-places-button");

	for (i = 0; i < 3; ++i) {
		g_object_set_data (
			G_OBJECT (priv->more_button [i]),
			"double-click-detector", double_click_detector_new ());

		g_signal_connect (
			G_OBJECT (priv->more_button [i]), "clicked",
			G_CALLBACK (more_button_clicked_cb), this);
	}
}

static void
set_panel_button_active (MainMenuUI *this, gboolean active)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	if (active) {
		gtk_window_present_with_time (
			GTK_WINDOW (priv->slab_window), gtk_get_current_event_time ());
		gtk_widget_set_state (priv->panel_button, GTK_STATE_ACTIVE);
	}
	else {
		gtk_widget_hide (priv->slab_window);
		gtk_widget_set_state (priv->panel_button, GTK_STATE_NORMAL);
	}
}

static void
select_page (MainMenuUI *this, gint page_id)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkToggleButton *selectors [3];

	gint i;


/* TODO: make this instant apply */

	if (page_id < 0)
		page_id = GPOINTER_TO_INT (libslab_get_gconf_value (CURRENT_PAGE_GCONF_KEY));
	else
		libslab_set_gconf_value (CURRENT_PAGE_GCONF_KEY, GINT_TO_POINTER (page_id));

	gtk_notebook_set_current_page (priv->file_section, page_id);

	selectors [0] = priv->apps_selector;
	selectors [1] = priv->docs_selector;
	selectors [2] = priv->dirs_selector;

	for (i = 0; i < 3; ++i)
		gtk_toggle_button_set_active (selectors [i], (i == page_id));
}

static void
update_limits (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GObject *user_tables   [2];
	GObject *recent_tables [2];
	gint     n_user_bins   [2];

	gint n_rows;
	gint n_cols;

	gint max_total_items_default;
	gint max_total_items_new;
	gint min_recent_items;

	gint i;


	user_tables [0] = G_OBJECT (priv->usr_apps_table);
	user_tables [1] = G_OBJECT (priv->usr_docs_table);

	recent_tables [0] = G_OBJECT (priv->rct_apps_table);
	recent_tables [1] = G_OBJECT (priv->rct_docs_table);

/* TODO: make this instant apply */

	max_total_items_default = GPOINTER_TO_INT (
		libslab_get_gconf_value (MAX_TOTAL_ITEMS_GCONF_KEY));
	min_recent_items = GPOINTER_TO_INT (
		libslab_get_gconf_value (MIN_RECENT_ITEMS_GCONF_KEY));

	priv->max_total_items = max_total_items_default;

	for (i = 0; i < 2; ++i) {
		g_object_get (user_tables [i], "n-rows", & n_rows, "n-columns", & n_cols, NULL);

		n_user_bins [i] = n_cols * n_rows;

		max_total_items_new = n_user_bins [i] + min_recent_items;

		if (priv->max_total_items < max_total_items_new)
			priv->max_total_items = max_total_items_new;
	}

	for (i = 0; i < 2; ++i)
		g_object_set (
			recent_tables [i],
			TILE_TABLE_LIMIT_PROP, priv->max_total_items - n_user_bins [i],
			NULL);
}

static void
connect_to_tile_triggers (MainMenuUI *this, TileTable *table)
{
	GList *tiles;
	GList *node;

	gulong handler_id;

	gint icon_width;


	g_object_get (G_OBJECT (table), TILE_TABLE_TILES_PROP, & tiles, NULL);

	for (node = tiles; node; node = node->next) {
		handler_id = g_signal_handler_find (
			G_OBJECT (node->data), G_SIGNAL_MATCH_FUNC, 0, 0,
			NULL, tile_action_triggered_cb, NULL);

		if (! handler_id)
			g_signal_connect (
				G_OBJECT (node->data), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);


		gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);
		gtk_widget_set_size_request (GTK_WIDGET (node->data), 6 * icon_width, -1);
	}
}

static void
hide_slab_if_urgent_close (MainMenuUI *this)
{
	if (! GPOINTER_TO_INT (libslab_get_gconf_value (URGENT_CLOSE_GCONF_KEY)))
		return;

	set_panel_button_active (this, FALSE);
}

static void
set_search_section_visible (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gchar **argv;
	gchar  *found_cmd = NULL;


	argv = get_search_argv (NULL);

	found_cmd = g_find_program_in_path (argv [0]);

	if (found_cmd) {
		gtk_widget_set_no_show_all (priv->search_section, FALSE);
		gtk_widget_show            (priv->search_section);
	}
	else {
		gtk_widget_set_no_show_all (priv->search_section, TRUE);
		gtk_widget_hide            (priv->search_section);
	}

	g_strfreev (argv);
	g_free (found_cmd);
}

static gchar **
get_search_argv (const gchar *search_txt)
{
	gchar  *cmd;
	gint    argc;
	gchar **argv_parsed = NULL;

	gchar **argv = NULL;

	gint i;


	cmd = (gchar *) libslab_get_gconf_value (SEARCH_CMD_GCONF_KEY);

	if (! cmd) {
		g_warning ("could not find search command in gconf [" SEARCH_CMD_GCONF_KEY "]\n");

		return NULL;
	}

	if (! g_shell_parse_argv (cmd, & argc, & argv_parsed, NULL))
		goto exit;

	argv = g_new0 (gchar *, argc + 1);

	for (i = 0; i < argc; ++i) {
		if (! strcmp (argv_parsed [i], "SEARCH_STRING"))
			argv [i] = g_strdup ((search_txt == NULL) ? "" : search_txt);
		else
			argv [i] = g_strdup (argv_parsed [i]);
	}

	argv [argc] = NULL;

exit:

	g_free (cmd);
	g_strfreev (argv_parsed);

	return argv;
}

static void
reorient_panel_button (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	PanelAppletOrient orientation;

	GtkWidget *child;


	orientation = panel_applet_get_orient (priv->panel_applet);

	child = gtk_bin_get_child (GTK_BIN (priv->panel_applet));

	if (GTK_IS_WIDGET (child))
		gtk_container_remove (GTK_CONTAINER (priv->panel_applet), child);

	switch (orientation) {
		case PANEL_APPLET_ORIENT_LEFT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT];
			break;

		case PANEL_APPLET_ORIENT_RIGHT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT];
			break;

		case PANEL_APPLET_ORIENT_UP:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM];
			break;

		default:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP];
			break;
	}

	gtk_container_add (GTK_CONTAINER (priv->panel_applet), priv->panel_button);
}

static void
launch_search (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	const gchar *search_txt;

	gchar **argv;
	gchar  *cmd;

	GError *error = NULL;


	search_txt = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

	argv = get_search_argv (search_txt);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, & error);

	if (error) {
		cmd = g_strjoinv (" ", argv);
		libslab_handle_g_error (
			& error, "%s: can't execute search [%s]\n", G_STRFUNC, cmd);
		g_free (cmd);
	}

	g_strfreev (argv);

	hide_slab_if_urgent_close (this);

	gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
}

static void
grab_pointer_and_keyboard (MainMenuUI *this, guint32 time)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GdkGrabStatus status;


	if (time == 0)
		time = GDK_CURRENT_TIME;

	if (GPOINTER_TO_INT (libslab_get_gconf_value (URGENT_CLOSE_GCONF_KEY))) {
		gtk_widget_grab_focus (priv->slab_window);
		gtk_grab_add          (priv->slab_window);

		status = gdk_pointer_grab (
			priv->slab_window->window, TRUE, GDK_BUTTON_PRESS_MASK,
			NULL, NULL, time);

		priv->ptr_is_grabbed = (status == GDK_GRAB_SUCCESS);

		status = gdk_keyboard_grab (priv->slab_window->window, TRUE, time);

		priv->kbd_is_grabbed = (status == GDK_GRAB_SUCCESS);
	}
	else {
		if (priv->ptr_is_grabbed) {
			gdk_pointer_ungrab (time);
			priv->ptr_is_grabbed = FALSE;
		}

		if (priv->kbd_is_grabbed) {
			gdk_keyboard_ungrab (time);
			priv->kbd_is_grabbed = FALSE;
		}

		gtk_grab_remove (priv->slab_window);
	}
}

static void
bind_beagle_search_key (MainMenuUI *this)
{
	xmlDocPtr  doc;
	xmlNodePtr node;

	gchar    *path;
	gchar    *contents;
	gsize     length;
	gboolean  success;

	gchar *key;

	gboolean ctrl;
	gboolean alt;

	xmlChar *val;


	path = g_build_filename (
		g_get_home_dir (), ".beagle/config/searching.xml", NULL);
	success = g_file_get_contents (path, & contents, & length, NULL);
	g_free (path);

	if (! success)
		return;

	doc = xmlParseMemory (contents, length);
	g_free (contents);

	if (! doc)
		return;

	if (! doc->children || ! doc->children->children)
		goto exit;

	for (node = doc->children->children; node; node = node->next) {
		if (! node->name || strcmp ((gchar *) node->name, "ShowSearchWindowBinding"))
			continue;

		if (! node->children)
			break;

		val  = xmlGetProp (node, (xmlChar *) "Ctrl");
		ctrl = val && ! strcmp ((gchar *) val, "true");
		xmlFree (val);

		val = xmlGetProp (node, (xmlChar *) "Alt");
		alt = val && ! strcmp ((gchar *) val, "true");
		xmlFree (val);

		for (node = node->children; node; node = node->next) {
			if (! node->name || strcmp ((gchar *) node->name, "Key"))
				continue;

			val = xmlNodeGetContent (node);

			key = g_strdup_printf (
				"%s%s%s", ctrl ? "<Ctrl>" : "", alt ? "<Alt>" : "", val);
			xmlFree (val);

			tomboy_keybinder_bind (key, search_tomboy_bindkey_cb, this);

			g_free (key);

			break;
		}

		break;
	}

exit:

	xmlFreeDoc (doc);
}

static gboolean
panel_button_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	DoubleClickDetector *dcd;


	if (event->button == 1) {
		dcd = DOUBLE_CLICK_DETECTOR (
			g_object_get_data (G_OBJECT (widget), "double-click-detector"));

		if (! double_click_detector_is_double_click (dcd, event->time, TRUE))
			set_panel_button_active (this, ! GTK_WIDGET_VISIBLE (priv->slab_window));

		return TRUE;
	}
	else
		g_signal_stop_emission_by_name (widget, "button_press_event");

	return FALSE;
}

static void
panel_button_drag_data_rcv_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                               GtkSelectionData *selection, guint info, guint time,
                               gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	gchar **uris;
	gint    uri_len;

	gint i;


	if (gtk_drag_get_source_widget (context))
		return;

	if (! selection)
		return;

	uris = gtk_selection_data_get_uris (selection);

	if (! uris)
		return;

	for (i = 0; uris [i]; ++i) {
		if (strncmp (uris [i], "file://", 7))
			continue;

		uri_len = strlen (uris [i]);

		if (! strcmp (& uris [i] [uri_len - 8], ".desktop"))
			tile_table_uri_added (priv->usr_apps_table, uris [i], (guint32) time);
		else
			tile_table_uri_added (priv->usr_docs_table, uris [i], (guint32) time);
	}

	g_strfreev (uris);
}

static gboolean
slab_window_expose_cb (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	cairo_t         *cr;
	cairo_pattern_t *gradient;


	cr = gdk_cairo_create (widget->window);

	cairo_rectangle (
		cr,
		event->area.x, event->area.y,
		event->area.width, event->area.height);

	cairo_clip (cr);

/* draw window background */

	cairo_rectangle (
		cr, 
		widget->allocation.x + 0.5, widget->allocation.y + 0.5,
		widget->allocation.width - 1, widget->allocation.height - 1);

	cairo_set_source_rgb (
		cr,
		widget->style->bg [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->bg [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->bg [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_fill_preserve (cr);

/* draw window outline */

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);

/* draw left pane background */

	cairo_rectangle (
		cr,
		priv->left_pane->allocation.x + 0.5, priv->left_pane->allocation.y + 0.5,
		priv->left_pane->allocation.width - 1, priv->left_pane->allocation.height - 1);

	cairo_set_source_rgb (
		cr,
		widget->style->bg [GTK_STATE_PRELIGHT].red   / 65535.0,
		widget->style->bg [GTK_STATE_PRELIGHT].green / 65535.0,
		widget->style->bg [GTK_STATE_PRELIGHT].blue  / 65535.0);

	cairo_fill_preserve (cr);

/* draw left pane outline */

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_stroke (cr);

/* draw top pane separator */

	cairo_move_to (
		cr,
		priv->top_pane->allocation.x + 0.5,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height - 0.5);

	cairo_line_to (
		cr,
		priv->top_pane->allocation.x + priv->top_pane->allocation.width - 0.5,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height - 0.5);

	cairo_set_source_rgb (
		cr,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0);

	cairo_stroke (cr);

/* draw top pane gradient */

	cairo_rectangle (
		cr,
		priv->top_pane->allocation.x + 0.5, priv->top_pane->allocation.y + 0.5,
		priv->top_pane->allocation.width - 1, priv->top_pane->allocation.height - 1);

	gradient = cairo_pattern_create_linear (
		priv->top_pane->allocation.x,
		priv->top_pane->allocation.y,
		priv->top_pane->allocation.x,
		priv->top_pane->allocation.y + priv->top_pane->allocation.height);
	cairo_pattern_add_color_stop_rgba (
		gradient, 0,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0,
		0.0);
	cairo_pattern_add_color_stop_rgba (
		gradient, 1,
		widget->style->dark [GTK_STATE_ACTIVE].red   / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].green / 65535.0,
		widget->style->dark [GTK_STATE_ACTIVE].blue  / 65535.0,
		0.2);

	cairo_set_source (cr, gradient);
	cairo_fill_preserve (cr);

	cairo_destroy (cr);

	return FALSE;
}

static gboolean
slab_window_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	MainMenuUI *this = MAIN_MENU_UI (user_data);

	switch (event->keyval) {
		case GDK_Escape:
			set_panel_button_active (this, FALSE);

			return TRUE;

		case GDK_W:
		case GDK_w:
			if (event->state & GDK_CONTROL_MASK) {
				set_panel_button_active (this, FALSE);

				return TRUE;
			}

		default:
			return FALSE;
	}
}

static gboolean
slab_window_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GdkWindow *ptr_window;

	DoubleClickDetector *dcd;
	
	
	ptr_window = gdk_window_at_pointer (NULL, NULL);

	if (ptr_window == priv->slab_window->window)
		return FALSE;

/* If the external click happens in the panel button then it's the user turning
 * off the menu (or double-clicking. */

	if (ptr_window == priv->panel_button->window) {
		dcd = DOUBLE_CLICK_DETECTOR (
			g_object_get_data (G_OBJECT (priv->panel_button), "double-click-detector"));

		if (! double_click_detector_is_double_click (dcd, event->time, TRUE))
			set_panel_button_active (this, FALSE);

		return FALSE;
	}

	hide_slab_if_urgent_close (this);

	return TRUE;
}

static void
slab_window_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GdkScreen *panel_button_screen;

	GdkRectangle button_geom;
	GdkRectangle slab_geom;
	GdkRectangle monitor_geom;

	PanelAppletOrient orient;


	gdk_window_get_origin (priv->panel_button->window, & button_geom.x, & button_geom.y);
	button_geom.width  = priv->panel_button->allocation.width;
	button_geom.height = priv->panel_button->allocation.height;

	slab_geom.width  = priv->slab_window->allocation.width;
	slab_geom.height = priv->slab_window->allocation.height;

	panel_button_screen = gtk_widget_get_screen (priv->panel_button);

	gdk_screen_get_monitor_geometry (
		panel_button_screen,
		gdk_screen_get_monitor_at_window (
			panel_button_screen, priv->panel_button->window),
		& monitor_geom);

	orient = panel_applet_get_orient (priv->panel_applet);

	switch (orient) {
		case PANEL_APPLET_ORIENT_UP:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y - slab_geom.height;
			break;

		case PANEL_APPLET_ORIENT_DOWN:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y + button_geom.height;
			break;

		case PANEL_APPLET_ORIENT_RIGHT:
			slab_geom.x = button_geom.x + button_geom.width;
			slab_geom.y = button_geom.y;
			break;

		case PANEL_APPLET_ORIENT_LEFT:
			slab_geom.x = button_geom.x - slab_geom.width;
			slab_geom.y = button_geom.y;
			break;

		default:
			slab_geom.x = 0;
			slab_geom.y = 0;
			break;
	}

	if (orient == PANEL_APPLET_ORIENT_UP || orient == PANEL_APPLET_ORIENT_DOWN) {
		if ((slab_geom.x + slab_geom.width) > (monitor_geom.x + monitor_geom.width))
			slab_geom.x = button_geom.x + button_geom.width - slab_geom.width;
	}
	else {
		if ((slab_geom.y + slab_geom.height) > (monitor_geom.y + monitor_geom.height))
			slab_geom.y = button_geom.y + button_geom.height - slab_geom.height;
	}

	gtk_window_move (GTK_WINDOW (priv->slab_window), slab_geom.x, slab_geom.y);
}

static void
slab_window_map_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	grab_pointer_and_keyboard (MAIN_MENU_UI (user_data), gdk_event_get_time (event));
}

static void
slab_window_unmap_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	if (priv->ptr_is_grabbed) {
		gdk_pointer_ungrab (gdk_event_get_time (event));
		priv->ptr_is_grabbed = FALSE;
	}

	if (priv->kbd_is_grabbed) {
		gdk_keyboard_ungrab (gdk_event_get_time (event));
		priv->kbd_is_grabbed = FALSE;
	}

	gtk_grab_remove (widget);
}

static gboolean
slab_window_grab_broken_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GdkEventGrabBroken *grab_event = (GdkEventGrabBroken *) event;
	gpointer window_data;


	if (grab_event->grab_window) {
		gdk_window_get_user_data (grab_event->grab_window, & window_data);

		if (GTK_IS_WIDGET (window_data))
			g_signal_connect (
				G_OBJECT (window_data), "event",
				G_CALLBACK (grabbing_window_event_cb), user_data);
	}

	return FALSE;
}

static void
search_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
	const gchar *entry_text = gtk_entry_get_text (entry);

	if (entry_text && strlen (entry_text) >= 1)
		launch_search (MAIN_MENU_UI (user_data));
}

static void
page_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	const gchar *name;

	gint page_id_new = 0;
	gint page_id_curr;


	name = gtk_widget_get_name (GTK_WIDGET (button));

	if (! libslab_strcmp (name, "slab-page-selector-button-applications"))
		page_id_new = 0;
	else if (! libslab_strcmp (name, "slab-page-selector-button-documents"))
		page_id_new = 1;
	else if (! libslab_strcmp (name, "slab-page-selector-button-places"))
		page_id_new = 2;
	else
		g_warning ("Unknown page selector [%s]\n", name);

	page_id_curr = GPOINTER_TO_INT (libslab_get_gconf_value (CURRENT_PAGE_GCONF_KEY));

	if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		if (page_id_new == page_id_curr)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		else
			return;
	}
	else
		select_page (MAIN_MENU_UI (user_data), page_id_new);
}

static void
tile_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);


	connect_to_tile_triggers (MAIN_MENU_UI (user_data), TILE_TABLE (g_obj));

	if (TILE_TABLE (g_obj) == priv->usr_apps_table)
		tile_table_reload (priv->rct_apps_table);
	else if (TILE_TABLE (g_obj) == priv->usr_docs_table)
		tile_table_reload (priv->rct_docs_table);
	else
		/* do nothing */ ;
}

static void
gtk_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	update_limits (MAIN_MENU_UI (user_data));
}

static void
tile_action_triggered_cb (Tile *tile, TileEvent *event, TileAction *action, gpointer user_data)
{
	if (! TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_NEW_WINDOW))
		return;

	hide_slab_if_urgent_close (MAIN_MENU_UI (user_data));
}

static void
more_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	DoubleClickDetector *dcd;

	GnomeDesktopItem *ditem;
	gchar            *ditem_id;


	dcd = DOUBLE_CLICK_DETECTOR (g_object_get_data (G_OBJECT (button), "double-click-detector"));

	if (! double_click_detector_is_double_click (dcd, gtk_get_current_event_time (), TRUE)) {
		if (GTK_WIDGET (button) == priv->more_button [MORE_APPS_BUTTON])
			ditem_id = libslab_get_gconf_value (APP_BROWSER_GCONF_KEY);
		else
			ditem_id = libslab_get_gconf_value (FILE_BROWSER_GCONF_KEY);

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (ditem_id);

		if (ditem) {
			libslab_gnome_desktop_item_launch_default (ditem);

			hide_slab_if_urgent_close (this);
		}
	}
}

static void
search_cmd_notify_cb (GConfClient *client, guint conn_id,
                      GConfEntry *entry, gpointer user_data)
{
	set_search_section_visible (MAIN_MENU_UI (user_data));
}

static void
panel_menu_open_cb (BonoboUIComponent *component, gpointer user_data, const gchar *verb)
{
	set_panel_button_active (MAIN_MENU_UI (user_data), TRUE);
}

static void
panel_menu_about_cb (BonoboUIComponent *component, gpointer user_data, const gchar *verb)
{
	MainMenuUI *this        = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);


	if (! priv->panel_about_dialog) {
		priv->panel_about_dialog = gtk_about_dialog_new ();

		g_object_set (priv->panel_about_dialog,
			"name", _("GNOME Main Menu"),
			"comments", _("The GNOME Main Menu"),
			"version", VERSION,
			"authors", main_menu_authors,
			"artists", main_menu_artists,
			"logo-icon-name", "gnome-fs-client",
			"copyright", "Copyright \xc2\xa9 2005-2007 Novell, Inc.",
			NULL);

		gtk_widget_show (priv->panel_about_dialog);

		g_signal_connect (G_OBJECT (priv->panel_about_dialog), "response",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}

	gtk_window_present (GTK_WINDOW (priv->panel_about_dialog));
}

static void
panel_applet_change_orient_cb (PanelApplet *applet, PanelAppletOrient orient, gpointer user_data)
{
	reorient_panel_button (MAIN_MENU_UI (user_data));
}

static void
panel_applet_change_background_cb (PanelApplet *applet, PanelAppletBackgroundType type, GdkColor *color,
                                   GdkPixmap *pixmap, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GtkRcStyle *rc_style;
	GtkStyle   *style;


/* reset style */

	gtk_widget_set_style (GTK_WIDGET (priv->panel_applet), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (priv->panel_applet), rc_style);
	gtk_rc_style_unref (rc_style);

	switch (type) {
		case PANEL_NO_BACKGROUND:
			break;

		case PANEL_COLOR_BACKGROUND:
			gtk_widget_modify_bg (
				GTK_WIDGET (priv->panel_applet), GTK_STATE_NORMAL, color);

			break;

		case PANEL_PIXMAP_BACKGROUND:
			style = gtk_style_copy (GTK_WIDGET (priv->panel_applet)->style);

			if (style->bg_pixmap [GTK_STATE_NORMAL])
				g_object_unref (style->bg_pixmap [GTK_STATE_NORMAL]);

			style->bg_pixmap [GTK_STATE_NORMAL] = g_object_ref (pixmap);

			gtk_widget_set_style (GTK_WIDGET (priv->panel_applet), style);

			g_object_unref (style);

			break;
	}
}

static void
slab_window_tomboy_bindkey_cb (gchar *key_string, gpointer user_data)
{
	set_panel_button_active (MAIN_MENU_UI (user_data), TRUE);
}

static void
search_tomboy_bindkey_cb (gchar *key_string, gpointer user_data)
{
	launch_search (MAIN_MENU_UI (user_data));
}

static gboolean
grabbing_window_event_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type == GDK_UNMAP || event->type == GDK_SELECTION_CLEAR)
		grab_pointer_and_keyboard (MAIN_MENU_UI (user_data), gdk_event_get_time (event));

	return FALSE;
}
