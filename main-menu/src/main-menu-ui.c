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

#include "tile.h"
#include "hard-drive-status-tile.h"
#include "network-status-tile.h"

#include "user-apps-tile-table.h"
#include "recent-apps-tile-table.h"
#include "user-docs-tile-table.h"
#include "recent-docs-tile-table.h"
#include "user-dirs-tile-table.h"
#include "system-tile-table.h"

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

	GtkWidget *panel_buttons [3];
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

	guint search_cmd_gconf_mntr_id;
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

static void    select_page                (MainMenuUI *, gint);
static void    update_limits              (MainMenuUI *);
static void    connect_to_tile_triggers   (MainMenuUI *, TileTable *);
static void    hide_window_on_launch      (MainMenuUI *);
static void    set_slab_window_visible    (MainMenuUI *, gboolean, guint32);
static void    set_search_section_visible (MainMenuUI *this);
static gchar **get_search_argv            (const gchar *);
static void    reorient_panel_button      (MainMenuUI *);

static void     panel_button_clicked_cb       (GtkButton *, gpointer);
static gboolean panel_button_button_press_cb  (GtkWidget *, GdkEventButton *, gpointer);
static gboolean slab_window_expose_cb         (GtkWidget *, GdkEventExpose *, gpointer);
static void     search_entry_activate_cb      (GtkEntry *, gpointer);
static void     page_button_clicked_cb        (GtkButton *, gpointer);
static void     tile_table_notify_cb          (GObject *, GParamSpec *, gpointer);
static void     gtk_table_notify_cb           (GObject *, GParamSpec *, gpointer);
static void     tile_action_triggered_cb      (Tile *, TileEvent *, TileAction *, gpointer);
static void     more_button_clicked_cb        (GtkButton *, gpointer);
static void     search_cmd_notify_cb          (GConfClient *, guint, GConfEntry *, gpointer);
static void     panel_menu_open_cb            (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_menu_about_cb           (BonoboUIComponent *, gpointer, const gchar *);
static void     panel_applet_change_orient_cb (PanelApplet *, PanelAppletOrient, gpointer);

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

	priv->main_menu_xml                             = NULL;
	priv->panel_button_xml                          = NULL;

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP]   = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT]  = NULL;
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT] = NULL;
	priv->panel_button                              = NULL;

	priv->slab_window                               = NULL;

	priv->top_pane                                  = NULL;
	priv->left_pane                                 = NULL;

	priv->search_section                            = NULL;
	priv->search_entry                              = NULL;

	priv->file_section                              = NULL;
	priv->apps_selector                             = NULL;
	priv->docs_selector                             = NULL;
	priv->dirs_selector                             = NULL;

	priv->sys_table                                 = NULL;
	priv->usr_apps_table                            = NULL;
	priv->rct_apps_table                            = NULL;
	priv->usr_docs_table                            = NULL;
	priv->rct_docs_table                            = NULL;
	priv->usr_dirs_table                            = NULL;

	priv->more_button [0]                           = NULL;
	priv->more_button [1]                           = NULL;
	priv->more_button [2]                           = NULL;

	priv->max_total_items                           = 10;

	priv->search_cmd_gconf_mntr_id                  = 0;
}

static void
main_menu_ui_finalize (GObject *g_obj)
{
	MainMenuUIPrivate *priv = PRIVATE (g_obj);

	gint i;


	for (i = 0; i < 3; ++i) {
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

	gint i;


	button_root = glade_xml_get_widget (
		priv->panel_button_xml, "slab-panel-button-root");

	gtk_widget_hide (button_root);

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-top");
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-left");
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT] = glade_xml_get_widget (
		priv->panel_button_xml, "slab-main-menu-panel-button-right");

	for (i = 0; i < 3; ++i) {
		g_object_set_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector",
			double_click_detector_new ());

		button_parent = gtk_widget_get_parent (priv->panel_buttons [i]);

		gtk_widget_ref (priv->panel_buttons [i]);
		gtk_container_remove (
			GTK_CONTAINER (button_parent), priv->panel_buttons [i]);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "clicked",
			G_CALLBACK (panel_button_clicked_cb), this);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "button_press_event",
			G_CALLBACK (panel_button_button_press_cb), this);
	}

	gtk_widget_destroy (button_root);

	reorient_panel_button (this);

	panel_applet_setup_menu_from_file (
		priv->panel_applet, NULL, "GNOME_MainMenu_ContextMenu.xml",
		NULL, applet_bonobo_verbs, this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_orient",
		G_CALLBACK (panel_applet_change_orient_cb), this);
}

static void
create_slab_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->slab_window = glade_xml_get_widget (
		priv->main_menu_xml, "slab-main-menu-window");
	gtk_widget_set_app_paintable (priv->slab_window, TRUE);
	gtk_widget_hide (priv->slab_window);

	priv->top_pane  = glade_xml_get_widget (priv->main_menu_xml, "top-pane");
	priv->left_pane = glade_xml_get_widget (priv->main_menu_xml, "left-pane");

	g_signal_connect (
		G_OBJECT (priv->slab_window), "expose-event",
		G_CALLBACK (slab_window_expose_cb), this);
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


	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "hard-drive-status-container"));
	tile = hard_drive_status_tile_new ();

	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));

	ctnr = GTK_CONTAINER (glade_xml_get_widget (
		priv->main_menu_xml, "network-status-container"));
	tile = network_status_tile_new ();

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


	g_object_get (G_OBJECT (table), TILE_TABLE_TILES_PROP, & tiles, NULL);

	for (node = tiles; node; node = node->next) {
		handler_id = g_signal_handler_find (
			G_OBJECT (node->data), G_SIGNAL_MATCH_FUNC, 0, 0,
			NULL, tile_action_triggered_cb, NULL);

		if (! handler_id)
			g_signal_connect (
				G_OBJECT (node->data), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);
	}
}

static void
hide_window_on_launch (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	if (! GPOINTER_TO_INT (libslab_get_gconf_value (URGENT_CLOSE_GCONF_KEY)))
		return;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), FALSE);
}

static void
set_slab_window_visible (MainMenuUI *this, gboolean visible, guint32 time_millis)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GTimeVal current_time;


	if (visible != GTK_WIDGET_VISIBLE (priv->slab_window)) {
		if (! visible)
			gtk_widget_hide (priv->slab_window);
		else {
			if (time_millis == 0) {
				g_get_current_time (& current_time);

				time_millis = 1000 * current_time.tv_sec + current_time.tv_usec / 1000;
			}

			gtk_widget_show_all (priv->slab_window);
		}
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), visible);
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
		if (search_txt && ! strcmp (argv_parsed [i], "SEARCH_STRING"))
			argv [i] = g_strdup (search_txt);
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

		default:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP];
			break;
	}

	gtk_container_add (GTK_CONTAINER (priv->panel_applet), priv->panel_button);
}

static void
panel_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (button);

	DoubleClickDetector *detector;
	GTimeVal current_time;
	guint32 current_time_millis;

	gboolean visible;


	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (toggle), "double-click-detector"));

	g_get_current_time (& current_time);

	current_time_millis = 1000 * current_time.tv_sec + current_time.tv_usec / 1000;

	visible = GTK_WIDGET_VISIBLE (priv->slab_window);

	if (! double_click_detector_is_double_click (detector, current_time_millis, TRUE))
		set_slab_window_visible (this, ! visible, current_time_millis);
	else
		set_slab_window_visible (this, visible, current_time_millis);
}

static gboolean
panel_button_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");

	return FALSE;
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

static void
search_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
	const gchar *search_txt;

	gchar **argv;
	gchar  *cmd;

	GError *error = NULL;


	search_txt = gtk_entry_get_text (entry);

	if (! search_txt || strlen (search_txt) < 1)
		return;

	argv = get_search_argv (search_txt);

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, & error);

	if (error) {
		cmd = g_strjoinv (" ", argv);
		libslab_handle_g_error (
			& error, "%s: can't execute search [%s]\n", __FUNCTION__, cmd);
		g_free (cmd);
	}

	g_strfreev (argv);

	gtk_entry_set_text (entry, "");

	hide_window_on_launch (MAIN_MENU_UI (user_data));
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

	hide_window_on_launch (MAIN_MENU_UI (user_data));
}

static void
more_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	DoubleClickDetector *detector;
	GTimeVal current_time;
	guint32 current_time_millis;

	GnomeDesktopItem *ditem;
	gchar            *ditem_id;


	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (button), "double-click-detector"));

	g_get_current_time (& current_time);

	current_time_millis = 1000 * current_time.tv_sec + current_time.tv_usec / 1000;

	if (! double_click_detector_is_double_click (detector, current_time_millis, TRUE)) {
		if (GTK_WIDGET (button) == priv->more_button [MORE_APPS_BUTTON])
			ditem_id = libslab_get_gconf_value (APP_BROWSER_GCONF_KEY);
		else
			ditem_id = libslab_get_gconf_value (FILE_BROWSER_GCONF_KEY);

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (ditem_id);

		if (ditem) {
			libslab_gnome_desktop_item_launch_default (ditem);

			hide_window_on_launch (this);
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
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (PRIVATE (user_data)->panel_button), TRUE);
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




#define HIDE 0
#if HIDE

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "main-menu-common.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <X11/Xlib.h>
#include <libgnomevfs/gnome-vfs.h>

#include "libslab-utils.h"

#include "application-tile.h"
#include "nameplate-tile.h"
#include "system-tile.h"
#include "document-tile.h"
#include "directory-tile.h"

#include "egg-recent-item.h"

#include "recent-files.h"

#include "double-click-detector.h"
#include "slab-gnome-util.h"
#include "gnome-utils.h"

#include "hard-drive-status-tile.h"
#include "network-status-tile.h"
#include "slab-window.h"
#include "tomboykeybinder.h"

#include "file-area.h"
#include "tile-table.h"

#include "main-menu-migration.h"

#define N_FILE_AREA_COLS             2
#define FILE_AREA_TABLE_COL_SPACINGS 6
#define FILE_AREA_TABLE_ROW_SPACINGS 6
#define N_FILE_CLASS                 3
#define TILE_WIDTH_SCALE             6

#define SYSTEM_TILE_SPACING 0

#define CURRENT_PAGE_GCONF_KEY "/desktop/gnome/applications/main-menu/file-area/file_class"

#define DISABLE_LOCK_SCREEN_GCONF_KEY  "/apps/panel/global/disable_lock_screen"
#define DISABLE_LOG_OUT_GCONF_KEY      "/apps/panel/global/disable_log_out"
#define DISABLE_TERMINAL_GCONF_KEY     "/desktop/gnome/lockdown/disable_command_line"

typedef enum {
	APPS_PAGE     = 0,
	DOCS_PAGE     = 1,
	DIRS_PAGE     = 2,
	PAGE_SENTINEL = 3
} PageID;

static const gchar *main_menu_authors[] = {
	"Jim Krehl <jimmyk@novell.com>",
	"Scott Reeves <sreeves@novell.com>",
	"Dan Winship <danw@novell.com>",
	NULL
};

static const gchar *main_menu_artists[] = {
	"Garrett LeSage <garrett@novell.com>",
	"Jakub Steiner <jimmac@novell.com>",
	NULL
};

static void main_menu_ui_class_init (MainMenuUIClass *);
static void main_menu_ui_init (MainMenuUI *);
static void main_menu_ui_dispose (GObject *);

static void bonobo_menu_menu_open_cb (BonoboUIComponent *, gpointer, const gchar *);
static void bonobo_menu_menu_about_cb (BonoboUIComponent *, gpointer, const gchar *);
static void applet_change_orient_cb (PanelApplet *, PanelAppletOrient, gpointer);
static void applet_change_background_cb (PanelApplet *, PanelAppletBackgroundType, GdkColor * color,
	GdkPixmap * pixmap, gpointer);

static void tomboy_bindkey_cb (gchar *, gpointer);

static void build_main_menu_window (MainMenuUI *);
static void build_panel_button (MainMenuUI *);

static void reorient_panel_button (MainMenuUI *);

static void hide_main_window (MainMenuUI *);
static void show_main_window (MainMenuUI *, guint32);
static void position_main_window (MainMenuUI *);

static gboolean main_window_key_press_cb (GtkWidget *, GdkEventKey *, gpointer);
static gboolean main_window_button_press_cb (GtkWidget *, GdkEventButton *, gpointer);
static void main_window_map_cb (GtkWidget *, GdkEvent *, gpointer);
static void main_window_unmap_cb (GtkWidget *, GdkEvent *, gpointer);
static void main_window_resize_cb (GtkWidget *, GtkAllocation *, gpointer);
static gboolean main_window_grab_broken_cb (GtkWidget *, GdkEvent *, gpointer);
static gboolean grabbing_window_event_cb (GtkWidget *, GdkEvent *, gpointer);

static void tile_drag_data_rcv_cb (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *,
	guint, guint, gpointer);

static gboolean panel_button_button_press_cb (GtkWidget *, GdkEventButton *, gpointer);
static gboolean panel_button_enter_notify_cb (GtkWidget *, GdkEventButton *, gpointer);

static GtkWidget *create_search_widget     (MainMenuUI *);
static void       search_entry_activate_cb (GtkEntry *, gpointer);

static GtkWidget *create_page_buttons    (MainMenuUI *);
static void       select_page            (MainMenuUI *, PageID);
static void       page_button_clicked_cb (GtkButton *, gpointer);

static GtkWidget *create_file_area_page      (MainMenuUI *, PageID);
static void       reload_user_table          (MainMenuUI *, TileTable *, PageID);
static void       tile_table_update_cb       (TileTable *, TileTableUpdateEvent *, gpointer);
static void       tile_table_uri_added_cb    (TileTable *, TileTableURIAddedEvent *, gpointer);
static void       more_button_clicked_cb     (GtkButton *, gpointer);

static void user_apps_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                        GnomeVFSMonitorEventType, gpointer);
static void user_docs_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                        GnomeVFSMonitorEventType, gpointer);

static void recent_files_store_changed_cb (MainMenuRecentMonitor *, gpointer);
static void reload_recent_apps_table (MainMenuUI *);
static void reload_recent_docs_table (MainMenuUI *);

static void create_system_table_widget   (MainMenuUI *);
static void reload_system_tile_table     (MainMenuUI *);
static void system_table_update_cb       (TileTable *, TileTableUpdateEvent *, gpointer);
static void system_table_uri_added_cb    (TileTable *, TileTableURIAddedEvent *, gpointer);
static void system_item_store_monitor_cb (GnomeVFSMonitorHandle *, const gchar *, const gchar *,
                                          GnomeVFSMonitorEventType, gpointer);

static void tile_activated_cb (Tile *, TileEvent *, gpointer);

static void bind_search_key (void);

static void grab_focus (MainMenuUI *, GdkEvent *);

static GtkWidget *get_section_header_label (const gchar *);
static void section_header_label_style_set (GtkWidget *, GtkStyle *, gpointer);

static void tile_action_triggered_cb (Tile *, TileEvent *, TileAction *, gpointer);

typedef struct {
	PanelApplet *applet;
	MainMenuConf *conf;
	MainMenuEngine *engine;

	GtkWidget *about_dialog;

	GtkWidget *main_window;
	GtkWidget *panel_button;

	GtkWidget *search_section;
	GtkEntry  *search_entry;

	GtkNotebook     *file_area_nb;
	gint             file_area_nb_ids [PAGE_SENTINEL];
	GtkToggleButton *page_btns        [PAGE_SENTINEL];

	TileTable *system_table;
	GnomeVFSMonitorHandle *system_item_monitor_handle;

	GnomeVFSMonitorHandle *file_area_monitor_handles [PAGE_SENTINEL];

	TileTable *user_apps_table;
	TileTable *user_docs_table;
	TileTable *user_dirs_table;

	TileTable *recent_apps_table;
	TileTable *recent_docs_table;
	TileTable *recent_dirs_table;
	MainMenuRecentMonitor *recent_monitor;

	gboolean ptr_is_grabbed;
	gboolean kbd_is_grabbed;
} MainMenuUIPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAIN_MENU_UI_TYPE, MainMenuUIPrivate))

static const BonoboUIVerb applet_bonobo_verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("MainMenuOpen", bonobo_menu_menu_open_cb),
	BONOBO_UI_UNSAFE_VERB ("MainMenuAbout", bonobo_menu_menu_about_cb),
	BONOBO_UI_VERB_END
};

static Atom atom_slab_main_menu_action = None;

GType
main_menu_ui_get_type (void)
{
	static GType object_type = 0;

	if (!object_type)
	{
		static const GTypeInfo object_info = {
			sizeof (MainMenuUIClass),
			NULL,
			NULL,
			(GClassInitFunc) main_menu_ui_class_init,
			NULL,
			NULL,
			sizeof (MainMenuUI),
			0,
			(GInstanceInitFunc) main_menu_ui_init
		};

		object_type = g_type_register_static (G_TYPE_OBJECT, "MainMenuUI", &object_info, 0);
	}

	return object_type;
}

static void
main_menu_ui_class_init (MainMenuUIClass * main_menu_ui_class)
{
	GObjectClass *g_obj_class = (GObjectClass *) main_menu_ui_class;

	g_obj_class->dispose = main_menu_ui_dispose;

	g_type_class_add_private (main_menu_ui_class, sizeof (MainMenuUIPrivate));
}

static void
main_menu_ui_init (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->recent_monitor = NULL;
}

static GdkFilterReturn
slab_action_filter (GdkXEvent * gdk_xevent, GdkEvent * event, gpointer user_data)
{
	MainMenuUI *ui = (MainMenuUI *) user_data;
	XEvent *xevent = (XEvent *) gdk_xevent;

	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.data.l[0] == atom_slab_main_menu_action)
		show_main_window (ui, GDK_CURRENT_TIME);
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

MainMenuUI *
main_menu_ui_new (PanelApplet * applet, MainMenuConf * conf, MainMenuEngine * engine)
{
	MainMenuUI *ui;
	MainMenuUIPrivate *priv;
	GdkAtom gdk_atom_slab_action;

	ui = g_object_new (MAIN_MENU_UI_TYPE, NULL);
	priv = PRIVATE (ui);

	priv->applet = applet;
	priv->conf = conf;
	priv->engine = engine;

	build_main_menu_window (ui);
	build_panel_button (ui);

	g_signal_connect (G_OBJECT (priv->applet), "change_orient",
		G_CALLBACK (applet_change_orient_cb), ui);

	g_signal_connect (G_OBJECT (priv->applet), "change_background",
		G_CALLBACK (applet_change_background_cb), ui);

	tomboy_keybinder_init ();
	tomboy_keybinder_bind ("<Ctrl>Escape", tomboy_bindkey_cb, ui);
	bind_search_key ();

	/* install message client handler to get messages through the X root window */
	gdk_atom_slab_action = gdk_atom_intern ("_SLAB_ACTION", FALSE);
	atom_slab_main_menu_action =
		XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
		"_SLAB_ACTION_MAIN_MENU", FALSE);
	gdk_display_add_client_message_filter (gdk_display_get_default (), gdk_atom_slab_action,
		slab_action_filter, ui);

	panel_applet_setup_menu_from_file (priv->applet, NULL, "GNOME_MainMenu_ContextMenu.xml",
		NULL, applet_bonobo_verbs, ui);

	return ui;
}

static void
main_menu_ui_dispose (GObject * obj)
{
}

void
main_menu_ui_release (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	gtk_widget_show_all (GTK_WIDGET (priv->applet));
}

void
main_menu_ui_close (MainMenuUI * ui, gboolean assured)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	if (assured || priv->conf->urgent_close)
		hide_main_window (ui);
}

void
main_menu_ui_reset_search (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	gtk_entry_set_text (priv->search_entry, "");
}

static void
bonobo_menu_menu_open_cb (BonoboUIComponent * component, gpointer user_data, const gchar * verb)
{
	show_main_window (MAIN_MENU_UI (user_data), GDK_CURRENT_TIME);
}

static void
bonobo_menu_menu_about_cb (BonoboUIComponent * component, gpointer user_data, const gchar * verb)
{
	MainMenuUI *main_menu_ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE (main_menu_ui);

	if (! priv->about_dialog) {
		priv->about_dialog = gtk_about_dialog_new ();

		g_object_set (priv->about_dialog,
			"name", _("GNOME Main Menu"),
			"comments", _("The GNOME Main Menu"),
			"version", VERSION,
			"authors", main_menu_authors,
			"artists", main_menu_artists,
			"logo-icon-name", "gnome-fs-client",
			"copyright", "Copyright \xc2\xa9 2005-2006 Novell, Inc.",
			NULL);

		gtk_widget_show (priv->about_dialog);

		g_signal_connect (G_OBJECT (priv->about_dialog), "delete-event",
			G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}

	if (priv->about_dialog)
		gtk_window_present (GTK_WINDOW (priv->about_dialog));
}

static void
applet_change_orient_cb (PanelApplet * applet, PanelAppletOrient orient, gpointer user_data)
{
	reorient_panel_button (MAIN_MENU_UI (user_data));
}

static void
applet_change_background_cb (PanelApplet * applet, PanelAppletBackgroundType type, GdkColor * color,
	GdkPixmap * pixmap, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE (ui);

	GtkRcStyle *rc_style;
	GtkStyle *style;

	/* reset style */
	gtk_widget_set_style (GTK_WIDGET (priv->applet), NULL);
	rc_style = gtk_rc_style_new ();
	gtk_widget_modify_style (GTK_WIDGET (priv->applet), rc_style);
	gtk_rc_style_unref (rc_style);

	switch (type)
	{
	case PANEL_NO_BACKGROUND:
		break;
	case PANEL_COLOR_BACKGROUND:
		gtk_widget_modify_bg (GTK_WIDGET (priv->applet), GTK_STATE_NORMAL, color);
		break;
	case PANEL_PIXMAP_BACKGROUND:
		style = gtk_style_copy (GTK_WIDGET (priv->applet)->style);
		if (style->bg_pixmap[GTK_STATE_NORMAL])
			g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
		style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
		gtk_widget_set_style (GTK_WIDGET (priv->applet), style);
		g_object_unref (style);
		break;
	}

	/* Alas, I don't know why this is necessary, but otherwise we get garbage
	 * the first time it draws.
	 */
	reorient_panel_button (ui);
}

static void
tomboy_bindkey_cb (gchar * key_string, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);

	show_main_window (ui, tomboy_keybinder_get_current_event_time ());
}

static void
build_main_menu_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *window;

	GtkWidget *search_widget;
	GtkWidget *page_buttons;

	GtkWidget *top_pane;
	GtkWidget *bottom_pane;
	GtkWidget *right_pane;

	GtkSizeGroup *right_pane_group;
	GtkSizeGroup *icon_group;
	GtkSizeGroup *label_group;

	GtkWidget *system_header;
	GtkWidget *status_header;
	GtkWidget *status_section;
	GtkWidget *hd_tile;
	GtkWidget *net_tile;

	GtkWidget *vbox;

	gint i;


	window = slab_window_new ();

	search_widget = create_search_widget (this);
	page_buttons  = create_page_buttons  (this);

	priv->file_area_nb = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_notebook_set_show_tabs   (priv->file_area_nb, FALSE);
	gtk_notebook_set_show_border (priv->file_area_nb, FALSE);

	for (i = 0; i < PAGE_SENTINEL; ++i)
		priv->file_area_nb_ids [i] = gtk_notebook_append_page (
			priv->file_area_nb, create_file_area_page (this, i), NULL);

	top_pane = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	vbox = gtk_vbox_new (FALSE, 6);

	gtk_container_add (GTK_CONTAINER (top_pane), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), search_widget, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), page_buttons,  FALSE, FALSE, 0);

	gtk_widget_set_name (top_pane, "slab-search-page-selector-pane");

	bottom_pane = GTK_WIDGET (priv->file_area_nb);

	right_pane_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	system_header = get_section_header_label (_("System"));
	gtk_misc_set_alignment (GTK_MISC (system_header), 0.0, 0.5);
	gtk_size_group_add_widget (right_pane_group, system_header);

	status_header = get_section_header_label (_("Status"));
	gtk_misc_set_alignment (GTK_MISC (status_header), 0.0, 0.5);
	gtk_size_group_add_widget (right_pane_group, status_header);

	icon_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	label_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	create_system_table_widget (this);
	gtk_size_group_add_widget (right_pane_group, GTK_WIDGET (priv->system_table));

	status_section = gtk_vbox_new (TRUE, 6);

	hd_tile = hard_drive_status_tile_new ();

	if (hd_tile)
	{
		g_signal_connect (G_OBJECT (hd_tile), "tile-action-triggered",
			G_CALLBACK (tile_action_triggered_cb), this);

		gtk_box_pack_start (GTK_BOX (status_section), hd_tile, FALSE, FALSE, 0);
	}

	net_tile = network_status_tile_new ();

	if (net_tile)
	{
		g_signal_connect (G_OBJECT (net_tile), "tile-action-triggered",
			G_CALLBACK (tile_action_triggered_cb), this);

		gtk_box_pack_start (GTK_BOX (status_section), net_tile, FALSE, FALSE, 0);
	}

	right_pane = gtk_vbox_new (FALSE, 12);

	if (priv->conf->lock_down_conf->system_area_visible) {
		gtk_box_pack_start (GTK_BOX (right_pane), system_header, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (right_pane), GTK_WIDGET (priv->system_table), FALSE, FALSE, 0);
	}

	if (priv->conf->lock_down_conf->status_area_visible) {
		gtk_box_pack_end (GTK_BOX (right_pane), status_section, FALSE, FALSE, 0);
		gtk_box_pack_end (GTK_BOX (right_pane), status_header, FALSE, FALSE, 0);
	}

	slab_window_set_contents (SLAB_WINDOW (window), top_pane, bottom_pane, right_pane);

	g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_widget_hide_on_delete),
		NULL);

	g_signal_connect (G_OBJECT (window), "key-press-event",
		G_CALLBACK (main_window_key_press_cb), this);

	g_signal_connect (G_OBJECT (window), "button_press_event",
		G_CALLBACK (main_window_button_press_cb), this);

	g_signal_connect (G_OBJECT (window), "size_allocate", G_CALLBACK (main_window_resize_cb),
		this);

	g_signal_connect (G_OBJECT (window), "grab-broken-event",
		G_CALLBACK (main_window_grab_broken_cb), this);

	g_signal_connect (G_OBJECT (window), "map_event", G_CALLBACK (main_window_map_cb), this);

	g_signal_connect (G_OBJECT (window), "unmap_event", G_CALLBACK (main_window_unmap_cb), this);

	gtk_drag_dest_set (GTK_WIDGET (window), GTK_DEST_DEFAULT_ALL, NULL, 0,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (window));

	g_signal_connect (G_OBJECT (window), "drag-data-received",
		G_CALLBACK (tile_drag_data_rcv_cb), this);

	priv->main_window = window;
	priv->search_section = search_widget;

	select_page (this, GPOINTER_TO_INT (libslab_get_gconf_value (CURRENT_PAGE_GCONF_KEY)));
}

static void
build_panel_button (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	priv->panel_button = gtk_toggle_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), FALSE);
	gtk_button_set_relief (GTK_BUTTON (priv->panel_button), GTK_RELIEF_NONE);

	gtk_widget_set_name (priv->panel_button, "slab-main-menu-panel-button");

	reorient_panel_button (ui);

	g_object_set_data (G_OBJECT (priv->panel_button), "double-click-detector",
		double_click_detector_new ());

	g_signal_connect (G_OBJECT (priv->panel_button), "button_press_event",
		G_CALLBACK (panel_button_button_press_cb), ui);

	g_signal_connect (G_OBJECT (priv->panel_button), "enter-notify-event",
		G_CALLBACK (panel_button_enter_notify_cb), NULL);

	gtk_drag_dest_set (GTK_WIDGET (priv->panel_button), GTK_DEST_DEFAULT_ALL, NULL, 0,
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (priv->panel_button));

	g_signal_connect (G_OBJECT (priv->panel_button), "drag-data-received",
		G_CALLBACK (tile_drag_data_rcv_cb), ui);

	gtk_container_add (GTK_CONTAINER (priv->applet), priv->panel_button);
}

static void
reorient_panel_button (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	PanelAppletOrient orientation;

	GtkWidget *button;
	GtkWidget *child;

	GtkWidget *box;
	GtkWidget *icon;
	GtkWidget *label;

	orientation = panel_applet_get_orient (priv->applet);

	button = priv->panel_button;

	if (!GTK_IS_BUTTON (button))
		return;

	if ((child = gtk_bin_get_child (GTK_BIN (button))))
		gtk_container_remove (GTK_CONTAINER (button), child);

	icon = gtk_image_new_from_icon_name ("gnome-fs-client", GTK_ICON_SIZE_MENU);
	label = gtk_label_new (_("Computer"));

	switch (orientation)
	{
	case PANEL_APPLET_ORIENT_UP:
	case PANEL_APPLET_ORIENT_DOWN:
		box = gtk_hbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);

		break;

	case PANEL_APPLET_ORIENT_LEFT:
		gtk_label_set_angle (GTK_LABEL (label), 90.0);

		box = gtk_vbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);

		break;

	case PANEL_APPLET_ORIENT_RIGHT:
		gtk_label_set_angle (GTK_LABEL (label), 270.0);

		box = gtk_vbox_new (FALSE, 6);
		gtk_box_pack_start (GTK_BOX (box), icon, 0, TRUE, TRUE);
		gtk_box_pack_start (GTK_BOX (box), label, 0, TRUE, TRUE);

		break;

	default:
		box = NULL;

		g_assert_not_reached ();

		break;
	}

	gtk_container_add (GTK_CONTAINER (button), box);
	gtk_widget_show_all (button);
}

static gboolean
main_window_key_press_cb (GtkWidget * widget, GdkEventKey * event, gpointer user_data)
{
	switch (event->keyval)
	{
	case GDK_Escape:
		hide_main_window (MAIN_MENU_UI (user_data));

		return TRUE;

	case GDK_W:
	case GDK_w:
		if (event->state & GDK_CONTROL_MASK)
		{
			hide_main_window (MAIN_MENU_UI (user_data));

			return TRUE;
		}

	default:
		return FALSE;
	}
}

static void
main_window_map_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);

	grab_focus (ui, event);
}

static void
main_window_unmap_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	if (priv->ptr_is_grabbed)
	{
		gdk_pointer_ungrab (gdk_event_get_time (event));
		priv->ptr_is_grabbed = FALSE;
	}

	if (priv->kbd_is_grabbed)
	{
		gdk_keyboard_ungrab (gdk_event_get_time (event));
		priv->kbd_is_grabbed = FALSE;
	}

	gtk_grab_remove (widget);
}

static void
main_window_resize_cb (GtkWidget * widget, GtkAllocation * alloc, gpointer user_data)
{
	position_main_window (MAIN_MENU_UI (user_data));
}

static gboolean
main_window_grab_broken_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	GdkEventGrabBroken *grab_event = (GdkEventGrabBroken *) event;
	gpointer window_data;

	if (grab_event->grab_window)
	{
		gdk_window_get_user_data (grab_event->grab_window, &window_data);

		if (GTK_IS_WIDGET (window_data))
			g_signal_connect (G_OBJECT (window_data), "event",
				G_CALLBACK (grabbing_window_event_cb), user_data);
	}

	return FALSE;
}

static gboolean
grabbing_window_event_cb (GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	if (event->type == GDK_UNMAP || event->type == GDK_SELECTION_CLEAR)
		grab_focus (MAIN_MENU_UI (user_data), event);

	return FALSE;
}

static void
tile_drag_data_rcv_cb (GtkWidget * widget, GdkDragContext * drag_context, gint x, gint y,
	GtkSelectionData * selection, guint info, guint time, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	gchar **uris;

	gint uri_len;

	gint i;

	if (gtk_drag_get_source_widget (drag_context))
		return;

	if (!selection)
		return;

	uris = gtk_selection_data_get_uris (selection);

	if (!uris)
		return;

	for (i = 0; uris && uris[i]; ++i)
	{
		uri_len = strlen (uris[i]);

		if (!strcmp (&uris[i][uri_len - 8], ".desktop"))
			main_menu_engine_add_user_app (priv->engine, uris[i]);
	}
}

static gboolean
main_window_button_press_cb (GtkWidget * widget, GdkEventButton * event, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE (ui);

	GdkWindow *ptr_window = gdk_window_at_pointer (NULL, NULL);

	if (priv->main_window->window != ptr_window)
	{
		if (priv->conf->urgent_close)
			hide_main_window (ui);

		return TRUE;
	}

	return FALSE;
}

static gboolean
panel_button_button_press_cb (GtkWidget * button, GdkEventButton * event, gpointer user_data)
{
	DoubleClickDetector *detector;

	if (event->button != 1)
	{
		g_signal_stop_emission_by_name (button, "button_press_event");

		return FALSE;
	}

	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (button), "double-click-detector"));

	if (double_click_detector_is_double_click (detector, event->time, TRUE))
		return TRUE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		hide_main_window (MAIN_MENU_UI (user_data));
	else
		show_main_window (MAIN_MENU_UI (user_data), event->time);

	return TRUE;
}

static gboolean
panel_button_enter_notify_cb (GtkWidget * button, GdkEventButton * event, gpointer user_data)
{
	return TRUE;
}

static void
hide_main_window (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), FALSE);

	gtk_widget_hide (priv->main_window);
}

static void
show_main_window (MainMenuUI * ui, guint32 timestamp)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	position_main_window (ui);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->panel_button), TRUE);

	gtk_widget_show_all (priv->main_window);
	gtk_window_present_with_time (GTK_WINDOW (priv->main_window), timestamp);

	if (! main_menu_engine_search_available (priv->engine))
		gtk_widget_hide_all (priv->search_section);
	else
		gtk_widget_grab_focus (GTK_WIDGET (priv->search_entry));
}

static void
position_main_window (MainMenuUI * ui)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	GtkWidget *panel_widget;
	GtkWidget *main_menu_widget;

	GtkAllocation panel_widget_alloc;
	GtkRequisition main_window_req;

	GdkScreen *screen;
	GdkRectangle monitor_dim;

	gint x_new, y_new;

	panel_widget = priv->panel_button;
	main_menu_widget = priv->main_window;

	panel_widget_alloc.width = panel_widget->allocation.width;
	panel_widget_alloc.height = panel_widget->allocation.height;

	gdk_window_get_origin (panel_widget->window, &panel_widget_alloc.x, &panel_widget_alloc.y);
	gtk_widget_size_request (main_menu_widget, &main_window_req);

	x_new = y_new = 0;

	switch (panel_applet_get_orient (priv->applet))
	{
	case PANEL_APPLET_ORIENT_UP:
		x_new = panel_widget_alloc.x;
		y_new = panel_widget_alloc.y - main_window_req.height;

		break;

	case PANEL_APPLET_ORIENT_DOWN:
		x_new = panel_widget_alloc.x;
		y_new = panel_widget_alloc.y + panel_widget_alloc.height;

		break;

	case PANEL_APPLET_ORIENT_LEFT:
		x_new = panel_widget_alloc.x - main_window_req.width;
		y_new = panel_widget_alloc.y;

		break;

	case PANEL_APPLET_ORIENT_RIGHT:
		x_new = panel_widget_alloc.x + panel_widget_alloc.width;
		y_new = panel_widget_alloc.y;

		break;

	default:
		break;
	}

	screen = gtk_widget_get_screen (panel_widget);
	gdk_screen_get_monitor_geometry (screen,
		gdk_screen_get_monitor_at_window (screen, panel_widget->window),
		&monitor_dim);

	if (x_new + main_window_req.width > monitor_dim.width)
		x_new = monitor_dim.width - main_window_req.width;

	if (x_new < 0)
		x_new = 0;

	if (y_new + main_window_req.height > monitor_dim.height)
		y_new = monitor_dim.height - main_window_req.height;

	if (y_new < 0)
		y_new = 0;

	gtk_window_move (GTK_WINDOW (main_menu_widget), x_new, y_new);
}

/*** BEGIN SEARCH WIDGET ***/

static GtkWidget *
create_search_widget (MainMenuUI *this)
{
/* TODO: add lock-down */

	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *hbox;
	GtkWidget *search_label;
	GtkWidget *search_entry;


	hbox = gtk_hbox_new (FALSE, 3);

	gtk_box_pack_start (
		GTK_BOX (hbox),
		gtk_image_new_from_icon_name ("system-search", GTK_ICON_SIZE_DND),
		FALSE, FALSE, 0);

	search_label = gtk_label_new_with_mnemonic (_("_Search:"));
	gtk_misc_set_alignment (GTK_MISC (search_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), search_label, FALSE, FALSE, 0);

	search_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), search_entry, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (search_label), search_entry);

	g_signal_connect (G_OBJECT (search_entry), "activate",
		G_CALLBACK (search_entry_activate_cb), this);

	priv->search_entry = GTK_ENTRY (search_entry);

	return hbox;
}

static void
search_entry_activate_cb (GtkEntry *entry_widget, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	main_menu_engine_execute_search (priv->engine, gtk_entry_get_text (entry_widget));
}

/*** END SEARCH WIDGET ***/

/*** BEGIN PAGE BUTTONS ***/

static GtkWidget *
create_page_buttons (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *hbox;

	gint i;


	priv->page_btns [APPS_PAGE] = GTK_TOGGLE_BUTTON (
		gtk_toggle_button_new_with_mnemonic (_("_Applications")));
	priv->page_btns [DOCS_PAGE] = GTK_TOGGLE_BUTTON (
		gtk_toggle_button_new_with_mnemonic (_("Doc_uments")));
	priv->page_btns [DIRS_PAGE] = GTK_TOGGLE_BUTTON (
		gtk_toggle_button_new_with_mnemonic (_("_Places")));

	hbox = gtk_hbox_new (FALSE, 6);

	for (i = 0; i < PAGE_SENTINEL; ++i) {
		gtk_widget_set_name (GTK_WIDGET (priv->page_btns [i]), "slab-page-selector-toggle-button");
		gtk_button_set_focus_on_click (GTK_BUTTON (priv->page_btns [i]), FALSE);

		g_object_set_data (
			G_OBJECT (priv->page_btns [i]), "page-id", GINT_TO_POINTER (i));

		g_signal_connect (
			G_OBJECT (priv->page_btns [i]), "clicked",
			G_CALLBACK (page_button_clicked_cb), this);

		gtk_box_pack_start (
			GTK_BOX (hbox), GTK_WIDGET (priv->page_btns [i]), FALSE, FALSE, 0);
	}

	return hbox;
}

static void
select_page (MainMenuUI *this, PageID page_id)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint i;


	libslab_set_gconf_value (CURRENT_PAGE_GCONF_KEY, GINT_TO_POINTER (page_id));

	gtk_notebook_set_current_page (
		priv->file_area_nb, priv->file_area_nb_ids [page_id]);

	for (i = 0; i < PAGE_SENTINEL; ++i)
		gtk_toggle_button_set_active (priv->page_btns [i], (i == page_id));
}

static void
page_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	PageID page_id_new;
	PageID page_id_curr;


	page_id_new  = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "page-id"));
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

/*** END PAGE BUTTONS ***/

/*** BEGIN FILE AREA ***/

static GtkWidget *
create_file_area_page (MainMenuUI *this, PageID page_id)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *file_area;
	GtkWidget *user_hdr;
	GtkWidget *recent_hdr;
	GtkWidget *more_button;

	GtkWidget *vbox;
	GtkWidget *alignment;


	file_area = file_area_new ();

	switch (page_id) {
		case APPS_PAGE:
			user_hdr   = gtk_label_new (_("Favorite Applications"));
			recent_hdr = gtk_label_new (_("Recent Applications"));

			more_button = gtk_button_new_with_label (_("More Applications..."));

			if (! priv->recent_monitor)
				priv->recent_monitor = main_menu_recent_monitor_new ();

			priv->user_apps_table   = FILE_AREA (file_area)->user_spec_table;
			priv->recent_apps_table = FILE_AREA (file_area)->recent_table;

			g_signal_connect (
				G_OBJECT (priv->recent_monitor), "changed",
				G_CALLBACK (recent_files_store_changed_cb), this);

			priv->file_area_monitor_handles [page_id] = libslab_add_apps_monitor (
				user_apps_store_monitor_cb, FILE_AREA (file_area)->user_spec_table);

			break;

		case DOCS_PAGE:
			user_hdr   = gtk_label_new (_("Favorite Documents"));
			recent_hdr = gtk_label_new (_("Recent Documents"));

			more_button = gtk_button_new_with_label (_("More Documents..."));

			if (! priv->recent_monitor)
				priv->recent_monitor = main_menu_recent_monitor_new ();

			priv->user_docs_table   = FILE_AREA (file_area)->user_spec_table;
			priv->recent_docs_table = FILE_AREA (file_area)->recent_table;

			priv->file_area_monitor_handles [page_id] = libslab_add_apps_monitor (
				user_docs_store_monitor_cb, FILE_AREA (file_area)->user_spec_table);

			break;

		case DIRS_PAGE:
			user_hdr   = gtk_label_new (_("Favorite Places"));
			recent_hdr = gtk_label_new (_("Recent Places"));

			more_button = gtk_button_new_with_label (_("More Places..."));

			break;

		default:
			break;
	}

	g_object_set (
		G_OBJECT (file_area),
		FILE_AREA_USER_HDR_PROP,         user_hdr,
		FILE_AREA_RECENT_HDR_PROP,       recent_hdr,
		FILE_AREA_MAX_TOTAL_ITEMS_PROP,  8,
		FILE_AREA_MIN_RECENT_ITEMS_PROP, 2,
		NULL);

	g_object_set_data (G_OBJECT (file_area), "page-id", GINT_TO_POINTER (page_id));
	g_object_set_data (G_OBJECT (FILE_AREA (file_area)->user_spec_table), "page-id", GINT_TO_POINTER (page_id));

	g_signal_connect (
		G_OBJECT (FILE_AREA (file_area)->user_spec_table), TILE_TABLE_UPDATE_SIGNAL,
		G_CALLBACK (tile_table_update_cb), this);

	g_signal_connect (
		G_OBJECT (FILE_AREA (file_area)->user_spec_table), TILE_TABLE_URI_ADDED_SIGNAL,
		G_CALLBACK (tile_table_uri_added_cb), GINT_TO_POINTER (page_id));

	g_signal_connect (
		G_OBJECT (more_button), "clicked",
		G_CALLBACK (more_button_clicked_cb), GINT_TO_POINTER (page_id));

	reload_user_table (this, FILE_AREA (file_area)->user_spec_table, page_id);

	switch (page_id) {
		case APPS_PAGE:
			reload_recent_apps_table (this);
			break;

		case DOCS_PAGE:
			reload_recent_docs_table (this);
			break;

		default:
			break;
	}

	alignment = gtk_alignment_new (1.0, 1.0, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), more_button);

	vbox = gtk_vbox_new (FALSE, 18);

	gtk_box_pack_start (GTK_BOX (vbox), file_area, TRUE, TRUE, 0);
	gtk_box_pack_end   (GTK_BOX (vbox), alignment, TRUE, TRUE, 0);

	return vbox;
}

static void
reload_user_table (MainMenuUI *this, TileTable *table, PageID page_id)
{
	GList *uris = NULL;

	GtkWidget *tile;
	GList     *tiles = NULL;

	GtkWidget   *image;
	gint         icon_width;

	gchar  *buf;
	gchar  *path;
	gchar **folders;

	GList *node;
	gint   i;


	switch (page_id) {
		case APPS_PAGE:
			uris = libslab_get_user_app_uris ();
			break;

		case DOCS_PAGE:
			uris = libslab_get_user_doc_uris ();
			break;

		case DIRS_PAGE:
			path = g_build_filename (g_get_home_dir (), ".gtk-bookmarks", NULL);

			buf = g_new0 (gchar, 1024);

			g_file_get_contents (path, & buf, NULL, NULL);

			folders = g_strsplit (buf, "\n", -1);

			for (i = 0; folders && folders [i]; ++i)
				if (strlen (folders [i]) > 0)
					uris = g_list_append (uris, g_strdup (folders [i]));

			g_strfreev (folders);
			g_free (path);
			g_free (buf);

			break;

		default:
			break;
	}

	for (node = uris; node; node = node->next) {
		switch (page_id) {
			case APPS_PAGE:
				tile = application_tile_new ((gchar *) node->data);
				break;

			case DOCS_PAGE:
				tile = document_tile_new ((gchar *) node->data, "text/plain", 0);
				break;

			case DIRS_PAGE:
				tile = directory_tile_new ((gchar *) node->data);
				break;

			default:
				break;
		}

		if (tile) {
			image = NAMEPLATE_TILE (tile)->image;

			gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);

			gtk_widget_set_size_request (tile, 6 * icon_width, -1);

			g_signal_connect (G_OBJECT (tile), "tile-activated",
				G_CALLBACK (tile_activated_cb), NULL);

			g_signal_connect (G_OBJECT (tile), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);

			tiles = g_list_append (tiles, tile);
		}

		g_free (node->data);
	}

	g_object_set (G_OBJECT (table), TILE_TABLE_TILES_PROP, tiles, NULL);

	g_list_free (uris);
	g_list_free (tiles);
}

static void
reload_recent_apps_table (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GList *files;
	GList *tiles = NULL;
	GList *user_uris;
	GList *sys_uris;

	MainMenuRecentFile *file;
	const gchar *desktop_item_url;

	gboolean blacklist;
	gboolean disable_term;

	GnomeDesktopItem *ditem;
	const gchar *categories;

	GtkWidget *tile;

	GList *node_i;
	GList *node_j;


	files = main_menu_get_recent_apps (priv->recent_monitor);
	user_uris = libslab_get_user_app_uris ();
	sys_uris  = libslab_get_system_item_uris ();

	disable_term = GPOINTER_TO_INT (libslab_get_gconf_value (DISABLE_TERMINAL_GCONF_KEY));

	for (node_i = files; node_i; node_i = node_i->next) {
		file = (MainMenuRecentFile *) node_i->data;

		desktop_item_url = main_menu_recent_file_get_uri (file);

		blacklist = FALSE;

		for (node_j = user_uris; !blacklist && node_j; node_j = node_j->next)
			if (! strcmp (desktop_item_url, (gchar *) node_j->data))
				blacklist = TRUE;

		for (node_j = sys_uris; !blacklist && node_j; node_j = node_j->next)
			if (! strcmp (desktop_item_url, (gchar *) node_j->data))
				blacklist = TRUE;

		if (! blacklist) {
			tile = application_tile_new (desktop_item_url);

			if (disable_term) {
				ditem = application_tile_get_desktop_item (APPLICATION_TILE (tile));
				categories = gnome_desktop_item_get_string (
						ditem, GNOME_DESKTOP_ITEM_CATEGORIES);

				if (categories && strstr (categories, "TerminalEmulator")) {
					gtk_widget_destroy (tile);
					tile = NULL;
				}
			}

			if (tile) {
				tiles = g_list_append (tiles, tile);

				g_signal_connect (
					G_OBJECT (tile), "tile-activated",
					G_CALLBACK (tile_activated_cb), NULL);

				g_signal_connect (
					G_OBJECT (tile), "tile-action-triggered",
					G_CALLBACK (tile_action_triggered_cb), this);
			}
		}

		g_object_unref (file);
	}

	g_object_set (G_OBJECT (priv->recent_apps_table), TILE_TABLE_TILES_PROP, tiles, NULL);
}

static void
reload_recent_docs_table (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GList *files;
	GList *tiles = NULL;

	MainMenuRecentFile *file;

	GtkWidget *tile;

	GList *node;


	files = main_menu_get_recent_files (priv->recent_monitor);

	for (node = files; node; node = node->next) {
		file = (MainMenuRecentFile *) node->data;

		tile = document_tile_new (
			main_menu_recent_file_get_uri       (file),
			main_menu_recent_file_get_mime_type (file),
			main_menu_recent_file_get_modified  (file));


		if (tile) {
			tiles = g_list_append (tiles, tile);

			g_signal_connect (
				G_OBJECT (tile), "tile-activated",
				G_CALLBACK (tile_activated_cb), NULL);

			g_signal_connect (
				G_OBJECT (tile), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);
		}

		g_object_unref (file);
	}

	g_object_set (G_OBJECT (priv->recent_docs_table), TILE_TABLE_TILES_PROP, tiles, NULL);
}

static void
tile_table_update_cb (TileTable *table, TileTableUpdateEvent *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	GList *tiles_new = NULL;

	GList    *node_u;
	GList    *node_v;
	gboolean  equal = FALSE;

	gint page_id;

	GList *node;


	if (g_list_length (event->tiles_prev) == g_list_length (event->tiles_curr)) {
		node_u = event->tiles_prev;
		node_v = event->tiles_curr;
		equal  = TRUE;

		while (equal && node_u && node_v) {
			if (tile_compare (node_u->data, node_v->data))
                		equal = FALSE;

			node_u = node_u->next;
			node_v = node_v->next;
		}
	}

	if (equal)
		return;

	for (node = event->tiles_curr; node; node = node->next)
		tiles_new = g_list_append (tiles_new, TILE (node->data)->uri);

	page_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (table), "page-id"));

	if (priv->file_area_monitor_handles [page_id])
		gnome_vfs_monitor_cancel (priv->file_area_monitor_handles [page_id]);

	switch (page_id) {
		case APPS_PAGE:
			libslab_save_app_uris (tiles_new);

			priv->file_area_monitor_handles [page_id] = libslab_add_apps_monitor (
				user_apps_store_monitor_cb, table);

			break;

		case DOCS_PAGE:
			libslab_save_doc_uris (tiles_new);

			priv->file_area_monitor_handles [page_id] = libslab_add_apps_monitor (
				user_docs_store_monitor_cb, table);

			break;

		default:
			break;
	}
}

static void
tile_table_uri_added_cb (TileTable *table, TileTableURIAddedEvent *event, gpointer user_data)
{
	GList *uris;
	gint uri_len;


	if (! event->uri)
		return;

	uri_len = strlen (event->uri);

	switch (GPOINTER_TO_INT (user_data)) {
		case APPS_PAGE:
			uris = libslab_get_user_app_uris ();
			uris = g_list_append (uris, event->uri);
			libslab_save_app_uris (uris);

			break;

		case DOCS_PAGE:
			uris = libslab_get_user_doc_uris ();
			uris = g_list_append (uris, event->uri);
			libslab_save_doc_uris (uris);

			break;

		default:
			break;
	}
}

static void
user_apps_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	reload_user_table (MAIN_MENU_UI (user_data), TILE_TABLE (user_data), APPS_PAGE);
}

static void
user_docs_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	reload_user_table (MAIN_MENU_UI (user_data), TILE_TABLE (user_data), DOCS_PAGE);
}

static void
recent_files_store_changed_cb (MainMenuRecentMonitor *manager, gpointer user_data)
{
	MainMenuUI *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE (this);

	reload_user_table (this, priv->user_apps_table, APPS_PAGE);
	reload_recent_apps_table (MAIN_MENU_UI (user_data));

	reload_user_table (this, priv->user_docs_table, DOCS_PAGE);
	reload_recent_docs_table (MAIN_MENU_UI (user_data));
}

static void
more_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	GnomeDesktopItem *ditem;

	if (GPOINTER_TO_INT (user_data) == APPS_PAGE)
		ditem = libslab_gnome_desktop_item_new_from_unknown_id ("application-browser.desktop");
	else
		ditem = libslab_gnome_desktop_item_new_from_unknown_id ("nautilus.desktop");

	libslab_gnome_desktop_item_launch_default (ditem);
}

/*** END FILE AREA ***/

/*** BEGIN SYSTEM AREA ***/

static void
create_system_table_widget (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *table;


	table = tile_table_new (1, TILE_TABLE_REORDERING_PUSH_PULL);
	g_object_set (G_OBJECT (table), "row-spacing", 3, NULL);

	g_signal_connect (
		G_OBJECT (table), TILE_TABLE_UPDATE_SIGNAL,
		G_CALLBACK (system_table_update_cb), NULL);

	g_signal_connect (
		G_OBJECT (table), TILE_TABLE_URI_ADDED_SIGNAL,
		G_CALLBACK (system_table_uri_added_cb), NULL);

	priv->system_item_monitor_handle = libslab_add_system_item_monitor (
		system_item_store_monitor_cb, this);

	priv->system_table = TILE_TABLE (table);

	reload_system_tile_table (this);
}

static void
reload_system_tile_table (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GList *uris;

	GtkWidget *tile;
	GList     *tiles = NULL;

	GList *node;


	uris = libslab_get_system_item_uris ();

	for (node = uris; node; node = node->next) {
		tile = system_tile_new ((gchar *) node->data);

		if (tile) {
			g_signal_connect (
				G_OBJECT (tile), "tile-activated",
				G_CALLBACK (tile_activated_cb), NULL);

			g_signal_connect (
				G_OBJECT (tile), "tile-action-triggered",
				G_CALLBACK (tile_action_triggered_cb), this);

			tiles = g_list_append (tiles, tile);
		}

		g_free (node->data);
	}

	g_object_set (G_OBJECT (priv->system_table), TILE_TABLE_TILES_PROP, tiles, NULL);

	g_list_free (tiles);
}

static void
system_table_update_cb (TileTable *table, TileTableUpdateEvent *event, gpointer user_data)
{
	GList *tiles_new = NULL;

	GList    *node_u;
	GList    *node_v;
	gboolean  equal = FALSE;

	GList *node;


	if (g_list_length (event->tiles_prev) == g_list_length (event->tiles_curr)) {
		node_u = event->tiles_prev;
		node_v = event->tiles_curr;
		equal  = TRUE;

		while (equal && node_u && node_v) {
			if (tile_compare (node_u->data, node_v->data))
                		equal = FALSE;

			node_u = node_u->next;
			node_v = node_v->next;
		}
	}

	if (equal)
		return;

	for (node = event->tiles_curr; node; node = node->next)
		tiles_new = g_list_append (tiles_new, TILE (node->data)->uri);

	libslab_save_system_item_uris (tiles_new);
}

static void
system_table_uri_added_cb (TileTable *table, TileTableURIAddedEvent *event, gpointer user_data)
{
	GList *uris;
	gint uri_len;


	if (! event->uri)
		return;

	uri_len = strlen (event->uri);

	if (! strcmp (& event->uri [uri_len - 8], ".desktop")) {
		uris = libslab_get_system_item_uris ();
		uris = g_list_append (uris, event->uri);
		libslab_save_system_item_uris (uris);
	}
}

static void
system_item_store_monitor_cb (
	GnomeVFSMonitorHandle *handle, const gchar *monitor_uri,
	const gchar *info_uri, GnomeVFSMonitorEventType type, gpointer user_data)
{
	reload_system_tile_table (MAIN_MENU_UI (user_data));
}

/*** END SYSTEM AREA ***/

static void
tile_activated_cb (Tile *tile, TileEvent *event, gpointer user_data)
{
	if (event->type == TILE_EVENT_ACTIVATED_DOUBLE_CLICK)
		return;

	tile_trigger_action_with_time (tile, tile->default_action, event->time);
}

static GtkWidget *
get_section_header_label (const gchar *markup)
{
	GtkWidget *label;
	gchar *text;


	text = g_strdup_printf ("%s", markup);

	label = gtk_label_new (text);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
	gtk_widget_set_name (label, "gnome-main-menu-section-header");

	g_signal_connect (G_OBJECT (label), "style-set",
		G_CALLBACK (section_header_label_style_set), NULL);

	g_free (text);

	return label;
}

static void
section_header_label_style_set (GtkWidget *widget, GtkStyle *prev_style, gpointer user_data)
{
	if (
		prev_style &&
		widget->style->fg [GTK_STATE_SELECTED].green ==
		prev_style->fg [GTK_STATE_SELECTED].green
	)
		return;

	gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, & widget->style->bg [GTK_STATE_SELECTED]);
}

static void
grab_focus (MainMenuUI * ui, GdkEvent * event)
{
	MainMenuUIPrivate *priv = PRIVATE (ui);

	GdkGrabStatus status;
	guint32 time;

	time = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;

	if (priv->conf->urgent_close)
	{
		gtk_widget_grab_focus (priv->main_window);
		gtk_grab_add (priv->main_window);

		status = gdk_pointer_grab (priv->main_window->window, TRUE, GDK_BUTTON_PRESS_MASK,
			NULL, NULL, time);

		priv->ptr_is_grabbed = (status == GDK_GRAB_SUCCESS);

		status = gdk_keyboard_grab (priv->main_window->window, TRUE, time);

		priv->kbd_is_grabbed = (status == GDK_GRAB_SUCCESS);
	}
	else
	{
		if (priv->ptr_is_grabbed)
		{
			gdk_pointer_ungrab (time);
			priv->ptr_is_grabbed = FALSE;
		}

		if (priv->kbd_is_grabbed)
		{
			gdk_keyboard_ungrab (time);
			priv->kbd_is_grabbed = FALSE;
		}

		gtk_grab_remove (priv->main_window);
	}
}

static void
tile_action_triggered_cb (Tile * tile, TileEvent * event, TileAction * action, gpointer user_data)
{
	MainMenuUI *ui = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE (ui);

	if (!priv->conf->urgent_close)
		return;

	if (TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_NEW_WINDOW)
		&& !TILE_ACTION_CHECK_FLAG (action, TILE_ACTION_OPENS_HELP))
		hide_main_window (ui);
}

static void
launch_search_cb (char *key, gpointer user_data)
{
	char *argv[] = { "beagle-search", NULL };

	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
bind_search_key (void)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *path, *contents, *key;
	gsize length;
	gboolean success, ctrl, alt;
	xmlChar *val;

	path = g_build_filename (g_get_home_dir (), ".beagle/config/searching.xml", NULL);
	success = g_file_get_contents (path, &contents, &length, NULL);
	g_free (path);

	if (!success)
		return;

	doc = xmlParseMemory (contents, length);
	g_free (contents);
	if (!doc)
		return;

	if (!doc->children || !doc->children->children)
	{
		xmlFreeDoc (doc);
		return;
	}

	for (node = doc->children->children; node; node = node->next)
	{
		if (!node->name || strcmp ((char *) node->name, "ShowSearchWindowBinding") != 0)
			continue;
		if (!node->children)
			break;

		val = xmlGetProp (node, (xmlChar *) "Ctrl");
		ctrl = val && !strcmp ((char *) val, "true");
		xmlFree (val);
		val = xmlGetProp (node, (xmlChar *) "Alt");
		alt = val && !strcmp ((char *) val, "true");
		xmlFree (val);

		for (node = node->children; node; node = node->next)
		{
			if (!node->name || strcmp ((char *) node->name, "Key") != 0)
				continue;
			val = xmlNodeGetContent (node);

			key = g_strdup_printf (
				"%s%s%s", ctrl ? "<Ctrl>" : "", alt ? "<Alt>" : "", val);
			xmlFree (val);

			tomboy_keybinder_bind (key, launch_search_cb, NULL);
			g_free (key);
			break;
		}
		break;
	}

	xmlFreeDoc (doc);
}

#endif
