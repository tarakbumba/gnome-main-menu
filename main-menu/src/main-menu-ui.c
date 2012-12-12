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

#include <mate-panel-applet.h>
#include <cairo.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <unistd.h>
#include <libslab/slab.h>

#include "hard-drive-status-tile.h"

#ifdef HAVE_NETWORK
#include "network-status-tile.h"
#endif

#include "tile-table.h"

#include "tomboykeybinder.h"

#define SETTINGS_SCHEMA                 "org.mate.gnome-main-menu"
#define URGENT_CLOSE_SETTINGS_KEY       "urgent-close"
#define APP_BROWSER_SETTINGS_KEY        "application-browser"
#define FILE_BROWSER_SETTINGS_KEY       "file-browser"
#define SEARCH_CMD_SETTINGS_KEY         "search-command"

#define FILE_AREA_SCHEMA                SETTINGS_SCHEMA ".file-area"
#define CURRENT_PAGE_SETTINGS_KEY       "file-class"
#define MAX_TOTAL_ITEMS_SETTINGS_KEY    "max-total-items"
#define MIN_RECENT_ITEMS_SETTINGS_KEY   "min-recent-items"
#define FILE_MGR_OPEN_SETTINGS_KEY      "file-mgr-open-cmd"
#define APP_BLACKLIST_SETTINGS_KEY      "file-blacklist"

#define LOCKDOWN_SETTINGS_SCHEMA        SETTINGS_SCHEMA ".lock-down"
#define MORE_LINK_VIS_SETTINGS_KEY      "application-browser-link-visible"
#define SEARCH_VIS_SETTINGS_KEY         "search-area-visible"
#define STATUS_VIS_SETTINGS_KEY         "status-area-visible"
#define SYSTEM_VIS_SETTINGS_KEY         "system-area-visible"
#define SHOWABLE_TYPES_SETTINGS_KEY     "showable-file-types"
#define MODIFIABLE_SYSTEM_SETTINGS_KEY  "user-modifiable-system-area"
#define MODIFIABLE_APPS_SETTINGS_KEY    "user-modifiable-apps"
#define MODIFIABLE_DOCS_SETTINGS_KEY    "user-modifiable-docs"
#define MODIFIABLE_DIRS_SETTINGS_KEY    "user-modifiable-dirs"

#define MATE_LOCKDOWN_SCHEMA            "org.mate.lockdown"
#define DISABLE_TERMINAL_SETTINGS_KEY   "disable-command-line"
#define DISABLE_LOCKSCREEN_SETTINGS_KEY "disable-lock-screen"

#define PANEL_SCHEMA                    "org.mate.panel"
#define DISABLE_LOGOUT_SETTINGS_KEY     "disable-log-out"

G_DEFINE_TYPE (MainMenuUI, main_menu_ui, G_TYPE_OBJECT)

typedef struct {
	MatePanelApplet *panel_applet;
	GtkWidget   *panel_about_dialog;

	GtkBuilder *main_menu_ui;
	GtkBuilder *panel_button_ui;

	GtkToggleButton *panel_buttons [4];
	GtkToggleButton *panel_button;

	GtkWidget *slab_window;

	GtkWidget *top_pane;
	GtkWidget *left_pane;

	GtkWidget *search_section;
	GtkWidget *search_entry;
	GtkWidget *network_status;

	GtkNotebook *file_section;
	GtkWidget   *page_selectors    [3];
	gint         notebook_page_ids [3];

	TileTable     *file_tables     [5];
	GtkWidget     *table_sections  [5];
	gboolean       allowable_types [5];

	TileTable *sys_table;

	GtkWidget *more_buttons  [3];
	GtkWidget *more_sections [3];

	gint max_total_items;

	GtkWidget *status_section;
	GtkWidget *system_section;

	BookmarkAgent *bm_agents [BOOKMARK_STORE_N_TYPES];

	GVolumeMonitor        *volume_mon;
	GList                 *mounts;

	GFileMonitor *recently_used_store_monitor;
	guint recently_used_timeout_id;

	GSettings *settings;
	GSettings *filearea_settings;
	GSettings *lockdown_settings;
	GSettings *mate_lockdown_settings;
	GSettings *panel_settings;

	gboolean ptr_is_grabbed;
	gboolean kbd_is_grabbed;

	guint recently_used_store_has_changed : 1;
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
static void setup_file_tables        (MainMenuUI *);
static void setup_bookmark_agents    (MainMenuUI *);
static void setup_lock_down          (MainMenuUI *);
static void setup_recently_used_store_monitor (MainMenuUI *this, gboolean is_startup);
static void update_recently_used_sections (MainMenuUI *this);

static void       select_page                (MainMenuUI *);
static void       update_limits              (MainMenuUI *);
static void       connect_to_tile_triggers   (MainMenuUI *, TileTable *);
static void       hide_slab_if_urgent_close  (MainMenuUI *);
static void       set_search_section_visible (MainMenuUI *);
static void       set_table_section_visible  (MainMenuUI *, TileTable *);
static gchar    **get_search_argv            (const gchar *);
static void       reorient_panel_button      (MainMenuUI *);
static void       bind_beagle_search_key     (MainMenuUI *);
static void       launch_search              (MainMenuUI *);
static void       grab_pointer_and_keyboard  (MainMenuUI *, guint32);
static void       apply_lockdown_settings    (MainMenuUI *);
static gboolean   app_is_in_blacklist        (const gchar *, MainMenuUI *);

static Tile *item_to_user_app_tile   (BookmarkItem *, gpointer);
static Tile *item_to_recent_app_tile (BookmarkItem *, gpointer);
static Tile *item_to_user_doc_tile   (BookmarkItem *, gpointer);
static Tile *item_to_recent_doc_tile (BookmarkItem *, gpointer);
static Tile *item_to_dir_tile        (BookmarkItem *, gpointer);
static Tile *item_to_system_tile     (BookmarkItem *, gpointer);
static BookmarkItem *app_uri_to_item (const gchar *, gpointer);
static BookmarkItem *doc_uri_to_item (const gchar *, gpointer);

static void     panel_button_clicked_cb           (GtkButton *, gpointer);
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
static void     more_buttons_clicked_cb            (GtkButton *, gpointer);
static void     search_cmd_notify_cb              (GSettings *, gchar *, gpointer);
static void     current_page_notify_cb            (GSettings *, gchar *, gpointer);
static void     lockdown_notify_cb                (GSettings *, gchar *, gpointer);
static void     panel_menu_open_cb                (GtkAction *, gpointer);
static void     panel_menu_about_cb               (GtkAction *, gpointer);
static void     panel_applet_change_orient_cb     (MatePanelApplet *, MatePanelAppletOrient, gpointer);
static void     panel_applet_change_background_cb (MatePanelApplet *, MatePanelAppletBackgroundType, GdkColor *,
                                                   GdkPixmap * pixmap, gpointer);
static void     slab_window_tomboy_bindkey_cb     (gchar *, gpointer);
static void     search_tomboy_bindkey_cb          (gchar *, gpointer);
static gboolean grabbing_window_event_cb          (GtkWidget *, GdkEvent *, gpointer);
static void     user_app_agent_notify_cb          (GObject *, GParamSpec *, gpointer);
static void     user_doc_agent_notify_cb          (GObject *, GParamSpec *, gpointer);
static void     volume_monitor_mount_cb           (GVolumeMonitor *, GMount *, gpointer);

static GdkFilterReturn slab_gdk_message_filter (GdkXEvent *, GdkEvent *, gpointer);

static const GtkActionEntry applet_actions [] = {
	{ "MainMenuOpen", GTK_STOCK_OPEN, N_("_Open Menu"),
		NULL, NULL,
		G_CALLBACK (panel_menu_open_cb) },
	{ "MainMenuAbout", GTK_STOCK_ABOUT, N_("_About"),
		NULL, NULL,
		G_CALLBACK (panel_menu_about_cb) }
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
	APPS_PAGE,
	DOCS_PAGE,
	DIRS_PAGE
};

enum {
	USER_APPS_TABLE,
	RCNT_APPS_TABLE,
	USER_DOCS_TABLE,
	RCNT_DOCS_TABLE,
	USER_DIRS_TABLE
};

enum {
	PANEL_BUTTON_ORIENT_TOP,
	PANEL_BUTTON_ORIENT_BOTTOM,
	PANEL_BUTTON_ORIENT_LEFT,
	PANEL_BUTTON_ORIENT_RIGHT
};

static Atom slab_action_main_menu_atom = None;

static gboolean
main_menu_delayed_setup (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	if (priv->recently_used_store_monitor != NULL)
		return FALSE; /* already setup */

	libslab_checkpoint ("main_menu_ui_new(): setup_recently_used_store_monitor");
	setup_recently_used_store_monitor (this, TRUE);
	libslab_checkpoint ("main_menu_ui_new(): setup_bookmark_agents");
	setup_bookmark_agents    (this);
	libslab_checkpoint ("main_menu_ui_new(): create_slab_window");
	create_slab_window       (this);
	libslab_checkpoint ("main_menu_ui_new(): create_search_section");
	create_search_section    (this);
	libslab_checkpoint ("main_menu_ui_new(): create_file_section");
	create_file_section      (this);
	libslab_checkpoint ("main_menu_ui_new(): create_user_apps_section");
	create_user_apps_section (this);
	libslab_checkpoint ("main_menu_ui_new(): create_rct_apps_section");
	create_rct_apps_section  (this);
	libslab_checkpoint ("main_menu_ui_new(): create_user_docs_section");
	create_user_docs_section (this);
	libslab_checkpoint ("main_menu_ui_new(): create_rct_docs_section");
	create_rct_docs_section  (this);
	libslab_checkpoint ("main_menu_ui_new(): create_user_dirs_section");
	create_user_dirs_section (this);
	libslab_checkpoint ("main_menu_ui_new(): create_system_section");
	create_system_section    (this);
	libslab_checkpoint ("main_menu_ui_new(): create_status_section");
	create_status_section    (this);
	libslab_checkpoint ("main_menu_ui_new(): create_more_buttons");
	create_more_buttons      (this);
	libslab_checkpoint ("main_menu_ui_new(): setup_file_tables");
	setup_file_tables        (this);
	libslab_checkpoint ("main_menu_ui_new(): setup_lock_down");
	setup_lock_down          (this);

	libslab_checkpoint ("main_menu_ui_new(): bind_beagle_search_key");
	bind_beagle_search_key  (this);
	libslab_checkpoint ("main_menu_ui_new(): select_page");
	select_page             (this);
	libslab_checkpoint ("main_menu_ui_new(): apply_lockdown_settings");
	apply_lockdown_settings (this);

	return FALSE;
}

MainMenuUI *
main_menu_ui_new (MatePanelApplet *applet)
{
	MainMenuUI        *this;
	MainMenuUIPrivate *priv;

	gchar *window_ui_path, *button_ui_path;


	this = g_object_new (MAIN_MENU_UI_TYPE, NULL);
	priv = PRIVATE (this);

	priv->panel_applet = applet;

	priv->settings = g_settings_new (SETTINGS_SCHEMA);
	priv->filearea_settings = g_settings_new (FILE_AREA_SCHEMA);
	priv->lockdown_settings = g_settings_new (LOCKDOWN_SETTINGS_SCHEMA);
	priv->mate_lockdown_settings = g_settings_new (MATE_LOCKDOWN_SCHEMA);
	priv->panel_settings = g_settings_new (PANEL_SCHEMA);

	window_ui_path = g_build_filename (DATADIR, PACKAGE, "slab-window.ui", NULL);
	button_ui_path = g_build_filename (DATADIR, PACKAGE, "slab-button.ui", NULL);

	priv->main_menu_ui    = gtk_builder_new ();
	gtk_builder_add_from_file (priv->main_menu_ui, window_ui_path, NULL);
	priv->panel_button_ui = gtk_builder_new ();
	gtk_builder_add_from_file (priv->panel_button_ui, button_ui_path, NULL);
	g_free (window_ui_path);
	g_free (button_ui_path);

	libslab_checkpoint ("main_menu_ui_new(): create_panel_button");
	create_panel_button (this);
	g_timeout_add_seconds (5, (GSourceFunc) main_menu_delayed_setup, this);

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

	priv->main_menu_ui                               = NULL;
	priv->panel_button_ui                            = NULL;

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
	priv->page_selectors [APPS_PAGE]                 = NULL;
	priv->page_selectors [DOCS_PAGE]                 = NULL;
	priv->page_selectors [DIRS_PAGE]                 = NULL;
	priv->notebook_page_ids [APPS_PAGE]              = 0;
	priv->notebook_page_ids [DOCS_PAGE]              = 0;
	priv->notebook_page_ids [DIRS_PAGE]              = 0;

	priv->file_tables [USER_APPS_TABLE]              = NULL;
	priv->file_tables [RCNT_APPS_TABLE]              = NULL;
	priv->file_tables [USER_DOCS_TABLE]              = NULL;
	priv->file_tables [RCNT_DOCS_TABLE]              = NULL;
	priv->file_tables [USER_DIRS_TABLE]              = NULL;
	priv->table_sections [USER_APPS_TABLE]           = NULL;
	priv->table_sections [RCNT_APPS_TABLE]           = NULL;
	priv->table_sections [USER_DOCS_TABLE]           = NULL;
	priv->table_sections [RCNT_DOCS_TABLE]           = NULL;
	priv->table_sections [USER_DIRS_TABLE]           = NULL;
	priv->allowable_types [USER_APPS_TABLE]          = TRUE;
	priv->allowable_types [RCNT_APPS_TABLE]          = TRUE;
	priv->allowable_types [USER_DOCS_TABLE]          = TRUE;
	priv->allowable_types [RCNT_DOCS_TABLE]          = TRUE;
	priv->allowable_types [USER_DIRS_TABLE]          = TRUE;

	priv->sys_table                                  = NULL;

	priv->more_buttons [APPS_PAGE]                   = NULL;
	priv->more_buttons [DOCS_PAGE]                   = NULL;
	priv->more_buttons [DIRS_PAGE]                   = NULL;
	priv->more_sections [APPS_PAGE]                  = NULL;
	priv->more_sections [DOCS_PAGE]                  = NULL;
	priv->more_sections [DIRS_PAGE]                  = NULL;

	priv->max_total_items                            = 8;

	priv->status_section                             = NULL;
	priv->system_section                             = NULL;
	priv->network_status                             = NULL;

	priv->volume_mon                                 = NULL;

	priv->settings                                   = NULL;
	priv->filearea_settings                          = NULL;
	priv->lockdown_settings                          = NULL;
	priv->mate_lockdown_settings                     = NULL;
	priv->panel_settings                             = NULL;

	priv->ptr_is_grabbed                             = FALSE;
	priv->kbd_is_grabbed                             = FALSE;
}

static void
main_menu_ui_finalize (GObject *g_obj)
{
	MainMenuUIPrivate *priv = PRIVATE (g_obj);

	gint i;

	if (priv->recently_used_store_monitor)
		g_file_monitor_cancel (priv->recently_used_store_monitor);

	for (i = 0; i < 4; ++i) {
		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->more_buttons [i]), "double-click-detector")));

		g_object_unref (G_OBJECT (g_object_get_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector")));

		g_object_unref (priv->panel_buttons [i]);
	}

	g_object_unref (priv->settings);
	g_object_unref (priv->filearea_settings);
	g_object_unref (priv->lockdown_settings);
	g_object_unref (priv->mate_lockdown_settings);
	g_object_unref (priv->panel_settings);

	for (i = 0; i < BOOKMARK_STORE_N_TYPES; ++i)
		g_object_unref (priv->bm_agents [i]);

	g_list_foreach (priv->mounts, (GFunc) g_object_unref, NULL);
	g_list_free (priv->mounts);
	g_object_unref (priv->volume_mon);

	G_OBJECT_CLASS (main_menu_ui_parent_class)->finalize (g_obj);
}

static void
create_panel_button (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *button_root;
	GtkWidget *button_parent;

	GtkActionGroup* action_group;
	gchar* ui_path;

	gint i;


	button_root = GTK_WIDGET (gtk_builder_get_object (
		priv->panel_button_ui, "slab-panel-button-root"));

	gtk_widget_hide (button_root);

	priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (
		priv->panel_button_ui, "slab-main-menu-panel-button-top"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (
		priv->panel_button_ui, "slab-main-menu-panel-button-bottom"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (
		priv->panel_button_ui, "slab-main-menu-panel-button-left"));
	priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT] = GTK_TOGGLE_BUTTON (gtk_builder_get_object (
		priv->panel_button_ui, "slab-main-menu-panel-button-right"));

	for (i = 0; i < 4; ++i) {
		gtk_widget_set_name (GTK_WIDGET (priv->panel_buttons [i]),
				     "slab-main-menu-panel-button");

		g_object_set_data (
			G_OBJECT (priv->panel_buttons [i]), "double-click-detector",
			double_click_detector_new ());

		button_parent = gtk_widget_get_parent (GTK_WIDGET (priv->panel_buttons [i]));

		g_object_ref (priv->panel_buttons [i]);
		gtk_container_remove (
			GTK_CONTAINER (button_parent), GTK_WIDGET (priv->panel_buttons [i]));

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "clicked",
			G_CALLBACK (panel_button_clicked_cb), this);

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "button_press_event",
			G_CALLBACK (panel_button_button_press_cb), this);

		gtk_drag_dest_set (
			GTK_WIDGET (priv->panel_buttons [i]),
			GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
		gtk_drag_dest_add_uri_targets (GTK_WIDGET (priv->panel_buttons [i]));

		g_signal_connect (
			G_OBJECT (priv->panel_buttons [i]), "drag-data-received",
			G_CALLBACK (panel_button_drag_data_rcv_cb), this);
	}

	gtk_widget_destroy (button_root);

	mate_panel_applet_set_flags (priv->panel_applet, MATE_PANEL_APPLET_EXPAND_MINOR);

	reorient_panel_button (this);

	action_group = gtk_action_group_new ("GNOME MainMenu Applet Actions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
		applet_actions, G_N_ELEMENTS (applet_actions), this);
	ui_path = g_build_filename (UIDIR, "GNOME_MainMenu_ContextMenu.xml", NULL);
	mate_panel_applet_setup_menu_from_file (priv->panel_applet, ui_path, action_group);
	g_free (ui_path);
	g_object_unref (action_group);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_orient",
		G_CALLBACK (panel_applet_change_orient_cb), this);

	g_signal_connect (
		G_OBJECT (priv->panel_applet), "change_background",
		G_CALLBACK (panel_applet_change_background_cb), this);
}

static GtkWidget *
get_widget (MainMenuUIPrivate *priv, const gchar *name)
{
	return GTK_WIDGET (gtk_builder_get_object (priv->main_menu_ui, name));
}


static void
create_slab_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GdkAtom slab_action_atom;


	priv->slab_window = get_widget (priv, "slab-main-menu-window");
	gtk_widget_set_app_paintable (priv->slab_window, TRUE);
	gtk_widget_hide              (priv->slab_window);
	gtk_window_stick             (GTK_WINDOW (priv->slab_window));

	priv->top_pane  = get_widget (priv, "top-pane");
	priv->left_pane = get_widget (priv, "left-pane");

	tomboy_keybinder_init ();
	tomboy_keybinder_bind ("<Ctrl>Escape", slab_window_tomboy_bindkey_cb, this);

	slab_action_atom = gdk_atom_intern ("_SLAB_ACTION", FALSE);
	slab_action_main_menu_atom = XInternAtom (
		GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "_SLAB_ACTION_MAIN_MENU", FALSE);
	gdk_display_add_client_message_filter (
		gdk_display_get_default (), slab_action_atom, slab_gdk_message_filter, this);

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

	priv->search_section = get_widget (priv, "search-section");
	priv->search_entry   = get_widget (priv, "search-entry");

	g_signal_connect (
		G_OBJECT (priv->search_entry), "activate",
		G_CALLBACK (search_entry_activate_cb), this);

	set_search_section_visible (this);

	g_signal_connect (priv->settings, "changed::" SEARCH_CMD_SETTINGS_KEY,
		G_CALLBACK (search_cmd_notify_cb), this);
}

static void
create_file_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkWidget *pages [3];

	gint i;


	priv->file_section = GTK_NOTEBOOK (gtk_builder_get_object (
		priv->main_menu_ui, "file-area-notebook"));

	priv->page_selectors [APPS_PAGE] =
        get_widget (priv, "slab-page-selector-button-applications");
	priv->page_selectors [DOCS_PAGE] =
        get_widget (priv, "slab-page-selector-button-documents");
	priv->page_selectors [DIRS_PAGE] =
        get_widget (priv, "slab-page-selector-button-places");

	pages [APPS_PAGE] = get_widget (priv, "applications-page");
	pages [DOCS_PAGE] = get_widget (priv, "documents-page");
	pages [DIRS_PAGE] = get_widget (priv, "places-page");

	for (i = 0; i < 3; ++i) {
		gtk_container_child_get (
			GTK_CONTAINER (priv->file_section), pages [i],
			"position", & priv->notebook_page_ids [i], NULL);

		g_object_set_data (
			G_OBJECT (priv->page_selectors [i]), "page-type", GINT_TO_POINTER (i));

		g_signal_connect (
			G_OBJECT (priv->page_selectors [i]), "clicked",
			G_CALLBACK (page_button_clicked_cb), this);
	}

	g_signal_connect (priv->filearea_settings, "changed::" CURRENT_PAGE_SETTINGS_KEY,
		G_CALLBACK (current_page_notify_cb), this);

	priv->table_sections [USER_APPS_TABLE] =
        get_widget (priv, "user-apps-section");
	priv->table_sections [RCNT_APPS_TABLE] =
        get_widget (priv, "recent-apps-section");
	priv->table_sections [USER_DOCS_TABLE] =
        get_widget (priv, "user-docs-section");
	priv->table_sections [RCNT_DOCS_TABLE] =
        get_widget (priv, "recent-docs-section");
	priv->table_sections [USER_DIRS_TABLE] =
        get_widget (priv, "user-dirs-section");
}

static void
create_system_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "system-item-table-container"));

	priv->sys_table = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_SYSTEM], -1, 1, TRUE, TRUE,
		item_to_system_tile, this, app_uri_to_item, NULL));

	connect_to_tile_triggers (this, priv->sys_table);

	gtk_container_add (ctnr, GTK_WIDGET (priv->sys_table));

	g_signal_connect (
		G_OBJECT (priv->sys_table), "notify::" TILE_TABLE_TILES_PROP,
		G_CALLBACK (tile_table_notify_cb), this);

	priv->system_section = get_widget (priv, "slab-system-section");
}

static void
create_status_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;
	GtkWidget    *tile;

	gint icon_width;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "hard-drive-status-container"));
	tile = hard_drive_status_tile_new ();

	gtk_icon_size_lookup (GTK_ICON_SIZE_DND, & icon_width, NULL);
	g_signal_connect (
		G_OBJECT (tile), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, tile);
	gtk_widget_show_all (GTK_WIDGET (ctnr));

#ifdef HAVE_NETWORK
	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "network-status-container"));
	priv->network_status = network_status_tile_new ();

	gtk_widget_set_size_request (priv->network_status, 6 * icon_width, -1);

	g_signal_connect (
		G_OBJECT (priv->network_status), "tile-action-triggered",
		G_CALLBACK (tile_action_triggered_cb), this);

	gtk_container_add   (ctnr, priv->network_status);
	gtk_widget_show_all (GTK_WIDGET (ctnr));
#endif

	priv->status_section = get_widget (priv, "slab-status-section");

}

static void
create_user_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "user-apps-table-container"));

	priv->file_tables [USER_APPS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_APPS], -1, 2, TRUE, TRUE,
		item_to_user_app_tile, this, app_uri_to_item, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_APPS_TABLE]));
}

static void
create_rct_apps_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "recent-apps-table-container"));

	priv->file_tables [RCNT_APPS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_RECENT_APPS], -1, 2, FALSE, FALSE,
		item_to_recent_app_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [RCNT_APPS_TABLE]));
}

static void
create_user_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "user-docs-table-container"));

	priv->file_tables [USER_DOCS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_DOCS], -1, 2, TRUE, TRUE,
		item_to_user_doc_tile, this, doc_uri_to_item, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_DOCS_TABLE]));
}

static void
create_rct_docs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "recent-docs-table-container"));

	priv->file_tables [RCNT_DOCS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_RECENT_DOCS], -1, 2, FALSE, FALSE,
		item_to_recent_doc_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [RCNT_DOCS_TABLE]));

	priv->volume_mon = g_volume_monitor_get ();
	priv->mounts = g_volume_monitor_get_mounts (priv->volume_mon);

	g_signal_connect (priv->volume_mon, "mount-added", G_CALLBACK (volume_monitor_mount_cb), this);
	g_signal_connect (priv->volume_mon, "mount-removed", G_CALLBACK (volume_monitor_mount_cb), this);
}

static void
create_user_dirs_section (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkContainer *ctnr;


	ctnr = GTK_CONTAINER (gtk_builder_get_object (
		priv->main_menu_ui, "user-dirs-table-container"));

	priv->file_tables [USER_DIRS_TABLE] = TILE_TABLE (tile_table_new (
		priv->bm_agents [BOOKMARK_STORE_USER_DIRS], -1, 2, FALSE, FALSE,
		item_to_dir_tile, this, NULL, NULL));

	gtk_container_add (ctnr, GTK_WIDGET (priv->file_tables [USER_DIRS_TABLE]));
}

static void
create_more_buttons (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint i;


	priv->more_buttons [0] = get_widget (priv, "more-applications-button");
	priv->more_buttons [1] = get_widget (priv, "more-documents-button");
	priv->more_buttons [2] = get_widget (priv, "more-places-button");

	for (i = 0; i < 3; ++i) {
		g_object_set_data (
			G_OBJECT (priv->more_buttons [i]),
			"double-click-detector", double_click_detector_new ());

		g_signal_connect (
			G_OBJECT (priv->more_buttons [i]), "clicked",
			G_CALLBACK (more_buttons_clicked_cb), this);
	}

	priv->more_sections [0] = get_widget (priv, "more-apps-section");
	priv->more_sections [1] = get_widget (priv, "more-docs-section");
	priv->more_sections [2] = get_widget (priv, "more-dirs-section");
}

static void
setup_file_tables (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GList *tiles;

	gint i;


	for (i = 0; i < 5; ++i) {
		g_object_set_data (G_OBJECT (priv->file_tables [i]), "table-id", GINT_TO_POINTER (i));
		gtk_table_set_row_spacings (GTK_TABLE (priv->file_tables [i]), 6);
		gtk_table_set_col_spacings (GTK_TABLE (priv->file_tables [i]), 6);

		connect_to_tile_triggers (this, priv->file_tables [i]);

		g_object_get (G_OBJECT (priv->file_tables [i]), TILE_TABLE_TILES_PROP, & tiles, NULL);

		if (g_list_length (tiles) <= 0)
			gtk_widget_hide (priv->table_sections [i]);

		g_signal_connect (
			G_OBJECT (priv->file_tables [i]), "notify::" TILE_TABLE_TILES_PROP,
			G_CALLBACK (tile_table_notify_cb), this);

		g_signal_connect (
			G_OBJECT (priv->file_tables [i]), "notify::n-rows",
			G_CALLBACK (gtk_table_notify_cb), this);
	}
}

static void
setup_bookmark_agents (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);
	gint i;

	for (i = 0; i < BOOKMARK_STORE_N_TYPES; ++i) {
		priv->bm_agents [i] = bookmark_agent_get_instance (i);

		if (i == BOOKMARK_STORE_USER_APPS || i == BOOKMARK_STORE_SYSTEM)
			g_signal_connect (
				G_OBJECT (priv->bm_agents [i]), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
				G_CALLBACK (user_app_agent_notify_cb), this);
		else if (i == BOOKMARK_STORE_USER_DOCS)
			g_signal_connect (
				G_OBJECT (priv->bm_agents [i]), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
				G_CALLBACK (user_doc_agent_notify_cb), this);
	}
}

static void
setup_lock_down (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	g_signal_connect (priv->lockdown_settings, "changed",
		G_CALLBACK (lockdown_notify_cb), this);
	g_signal_connect (priv->mate_lockdown_settings, "changed::" DISABLE_TERMINAL_SETTINGS_KEY,
		G_CALLBACK (lockdown_notify_cb), this);
	g_signal_connect (priv->mate_lockdown_settings, "changed::" DISABLE_LOCKSCREEN_SETTINGS_KEY,
		G_CALLBACK (lockdown_notify_cb), this);
	g_signal_connect (priv->panel_settings, "changed:" DISABLE_LOGOUT_SETTINGS_KEY,
		G_CALLBACK (lockdown_notify_cb), this);
}

static char *
get_recently_used_store_filename (void)
{
	const char *basename;

	basename = ".recently-used.xbel";

	return g_build_filename (g_get_home_dir (), basename, NULL);
}

#define RECENTLY_USED_STORE_THROTTLE_MILLISECONDS 2000

static void recently_used_store_monitor_changed_cb (GFileMonitor *monitor,
						    GFile *f1, GFile *f2,
						    GFileMonitorEvent event_type,
						    gpointer data)
{
	MainMenuUI *this = MAIN_MENU_UI (data);
	MainMenuUIPrivate *priv = PRIVATE (this);

	priv->recently_used_store_has_changed = TRUE;
	update_recently_used_sections (this);
}

static gboolean
setup_recently_used_throttle_timeout (gpointer data)
{
	MainMenuUI *this = data;
	MainMenuUIPrivate *priv = PRIVATE (this);

	update_recently_used_sections (this);
	priv->recently_used_timeout_id = 0;

	return FALSE;
}

/* Creates a GFileMonitor for the recently-used store, so we can be informed
 * when the store changes.
 */
static void
setup_recently_used_store_monitor (MainMenuUI *this, gboolean is_startup)
{
	MainMenuUIPrivate *priv = PRIVATE (this);
	char *path;
	GFile *file;
	GFileMonitor *monitor;

	priv->recently_used_store_has_changed = TRUE; /* ensure the store gets read the first time we need it */

	path = get_recently_used_store_filename ();
	file = g_file_new_for_path (path);
	g_free (path);

	monitor = g_file_monitor_file (file, 0, NULL, NULL);
	if (monitor) {
		g_file_monitor_set_rate_limit (monitor,
					       RECENTLY_USED_STORE_THROTTLE_MILLISECONDS);
		g_signal_connect (monitor, "changed",
				  G_CALLBACK (recently_used_store_monitor_changed_cb),
				  this);
	}

	g_object_unref (file);

	priv->recently_used_store_monitor = monitor;

	if (priv->recently_used_timeout_id != 0)
		g_source_remove (priv->recently_used_timeout_id);

	if (is_startup)
		priv->recently_used_timeout_id = g_idle_add (setup_recently_used_throttle_timeout, this);
	else
		priv->recently_used_timeout_id = g_timeout_add_seconds (RECENTLY_USED_STORE_THROTTLE_MILLISECONDS / 1000, /* 2 seconds */
									setup_recently_used_throttle_timeout, this);
}

static Tile *
item_to_user_app_tile (BookmarkItem *item, gpointer data)
{
	if (app_is_in_blacklist (item->uri, data))
		return NULL;

	return TILE (application_tile_new (item->uri));
}

static Tile *
item_to_recent_app_tile (BookmarkItem *item, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	Tile *tile = NULL;

	gboolean blacklisted;


	blacklisted =
		bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_SYSTEM],    item->uri) ||
		bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_USER_APPS], item->uri) ||
		app_is_in_blacklist (item->uri, data);

	if (! blacklisted)
		tile = TILE (application_tile_new (item->uri));

	return tile;
}

static Tile *
item_to_user_doc_tile (BookmarkItem *item, gpointer data)
{
	//force an icon since the thumbnail is basically blank - BNC#373783
	if (item->title && (!strcmp (item->title, "BLANK_SPREADSHEET") || !strcmp (item->title, "BLANK_DOCUMENT")))
	{
		if (! strcmp (item->title, "BLANK_SPREADSHEET"))
			return TILE (document_tile_new_force_icon (item->uri, item->mime_type,
				item->mtime, "gnome-mime-application-vnd.oasis.opendocument.spreadsheet-template"));
		else
			return TILE (document_tile_new_force_icon (item->uri, item->mime_type,
				item->mtime, "gnome-mime-application-vnd.oasis.opendocument.text-template"));
	}
	return TILE (document_tile_new (BOOKMARK_STORE_USER_DOCS, item->uri, item->mime_type, item->mtime));
}

static Tile *
item_to_recent_doc_tile (BookmarkItem *item, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	GMount         *mount;
	GVolume        *volume;
	GFile          *file;
	char           *nfs_id;
	gboolean        is_nfs = FALSE;
	gboolean        is_local = TRUE;

	GList *node;

	/* we no longer do our own thumbnailing so this "should" not hang on stale mounts, slow endpoints ... */
	/*
	if (! g_str_has_prefix (item->uri, "file://"))
		return NULL;

	for (node = priv->mounts; ! is_nfs && node; node = node->next) {
		mount = node->data;
		volume = g_mount_get_volume (mount); //need to check for null here

		nfs_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_NFS_MOUNT);
		is_nfs = (nfs_id != NULL);

		g_free (nfs_id);
		g_object_unref (volume);
	}

	if (is_nfs)
		return NULL;

	file = g_file_new_for_uri (item->uri);
	is_local = g_file_is_native (file);
	g_object_unref (file);

	if (! is_local)
		return NULL;
	*/

	if (bookmark_agent_has_item (priv->bm_agents [BOOKMARK_STORE_USER_DOCS], item->uri))
		return NULL;

	return TILE (document_tile_new (BOOKMARK_STORE_RECENT_DOCS, item->uri, item->mime_type, item->mtime));
}

static Tile *
item_to_dir_tile (BookmarkItem *item, gpointer data)
{
	return TILE (directory_tile_new (item->uri, item->title, item->icon, item->mime_type));
}

static Tile *
item_to_system_tile (BookmarkItem *item, gpointer data)
{
	Tile  *tile;
	gchar *basename;
	gchar *translated_title;

	if (app_is_in_blacklist (item->uri, data))
		return NULL;
	
	translated_title = item->title ? _(item->title) : NULL;

	tile = TILE (system_tile_new (item->uri, translated_title));

	/* with 0.9.12 we start with clean system area - see move_system_area_to_new_set
	   no longer need this code to clean up old junk
	if (tile)
		return tile;

	basename = g_strrstr (item->uri, "/");
	if (basename)
		basename++;
	else
		basename = item->uri;

	if (! libslab_strcmp (basename, "control-center.desktop"))
		tile = TILE (system_tile_new ("gnomecc.desktop", translated_title));
	else if (! libslab_strcmp (basename, "zen-installer.desktop"))
		tile = TILE (system_tile_new ("package-manager.desktop", translated_title));
	*/

	return tile;
}

static BookmarkItem *
app_uri_to_item (const gchar *uri, gpointer data)
{
	BookmarkItem *item = g_new0 (BookmarkItem, 1);

	item->uri       = g_strdup (uri);
	item->mime_type = g_strdup ("application/x-desktop");

	return item;
}

static BookmarkItem *
doc_uri_to_item (const gchar *uri, gpointer data)
{
	BookmarkItem *item;
	GFile *file;
	GFileInfo *info;
	GAppInfo *default_app;

	item = g_new0 (BookmarkItem, 1);

	item->uri = g_strdup (uri);

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED ","
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  0, NULL, NULL);
	g_object_unref (file);

	if (!info) {
		item->mtime = 0;
		item->mime_type = NULL;
	} else {
		item->mtime = (time_t) g_file_info_get_attribute_uint64 (info,
									 G_FILE_ATTRIBUTE_TIME_MODIFIED);
		item->mime_type = g_strdup (g_file_info_get_content_type (info));
		g_object_unref (info);
	}

	if (item->mime_type) {
		default_app = g_app_info_get_default_for_type (item->mime_type, FALSE);

		item->app_name = g_strdup (g_app_info_get_name (default_app));
		item->app_exec = g_strdup (g_app_info_get_executable (default_app));

		g_object_unref (default_app);
	}

	if (! (item->mime_type && item->app_name)) {
		bookmark_item_free (item);
		item = NULL;
	}

	return item;
}

static gboolean
app_is_in_blacklist (const gchar *uri, MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);
	gchar **blacklist;
	gint i;

	gboolean disable_term;
	gboolean disable_logout;
	gboolean disable_lockscreen;

	gboolean blacklisted;

	disable_term = g_settings_get_boolean (priv->mate_lockdown_settings, DISABLE_TERMINAL_SETTINGS_KEY);
	blacklisted  = disable_term && libslab_desktop_item_is_a_terminal (uri);

	if (blacklisted)
		return TRUE;

	disable_logout = g_settings_get_boolean (priv->panel_settings, DISABLE_LOGOUT_SETTINGS_KEY);
	blacklisted    = disable_logout && libslab_desktop_item_is_logout (uri);

	if (blacklisted)
		return TRUE;

	disable_lockscreen = g_settings_get_boolean (priv->lockdown_settings, DISABLE_LOCKSCREEN_SETTINGS_KEY);
	/* Dont allow lock screen if root - same as gnome-panel */
	blacklisted = libslab_desktop_item_is_lockscreen (uri) &&
		( (geteuid () == 0) || disable_lockscreen );

	if (blacklisted)
		return TRUE;

	blacklist = g_settings_get_strv (priv->filearea_settings, APP_BLACKLIST_SETTINGS_KEY);

	for (i = 0; blacklist[i] != NULL; i++) {
		if (! blacklisted && strstr (uri, blacklist[i]))
			blacklisted = TRUE;
	}

	if (blacklist)
		g_strfreev (blacklist);

	return blacklisted;
}

static void
select_page (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GtkToggleButton *button;
	gint curr_page;


	curr_page = g_settings_get_int (priv->filearea_settings, CURRENT_PAGE_SETTINGS_KEY);
	button    = GTK_TOGGLE_BUTTON (priv->page_selectors [curr_page]);

	gtk_toggle_button_set_active (button, TRUE);
}

static void
update_limits (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	GObject   *user_tables   [2];
	GObject   *recent_tables [2];
	gint       n_user_bins   [2];
	GtkWidget *sections      [2];

	gint n_rows;
	gint n_cols;

	gint max_total_items_default;
	gint max_total_items_new;
	gint min_recent_items;

	gint i;


	user_tables [0] = G_OBJECT (priv->file_tables [USER_APPS_TABLE]);
	user_tables [1] = G_OBJECT (priv->file_tables [USER_DOCS_TABLE]);

	recent_tables [0] = G_OBJECT (priv->file_tables [RCNT_APPS_TABLE]);
	recent_tables [1] = G_OBJECT (priv->file_tables [RCNT_DOCS_TABLE]);

	sections [0] = priv->table_sections [USER_APPS_TABLE];
	sections [1] = priv->table_sections [USER_DOCS_TABLE];

/* TODO: make this instant apply */

	max_total_items_default =
		g_settings_get_int (priv->filearea_settings, MAX_TOTAL_ITEMS_SETTINGS_KEY);
	min_recent_items =
		g_settings_get_int (priv->filearea_settings, MIN_RECENT_ITEMS_SETTINGS_KEY);

	priv->max_total_items = max_total_items_default;

	for (i = 0; i < 2; ++i) {
		if (gtk_widget_get_visible (sections [i]))
			g_object_get (
				user_tables [i],
				"n-rows", & n_rows, "n-columns", & n_cols, NULL);
		else
			n_rows = n_cols = 0;

		n_user_bins [i] = n_cols * n_rows;

		max_total_items_new = n_user_bins [i] + min_recent_items;

		if (priv->max_total_items < max_total_items_new)
			priv->max_total_items = max_total_items_new;
	}

	g_object_get (
		priv->file_tables [USER_DIRS_TABLE],
		"n-rows", & n_rows, "n-columns", & n_cols, NULL);

	if (priv->max_total_items < (n_rows * n_cols))
		priv->max_total_items = n_rows * n_cols;

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
	MainMenuUIPrivate *priv = PRIVATE (this);

	if (! g_settings_get_boolean (priv->settings, URGENT_CLOSE_SETTINGS_KEY))
		return;

	gtk_toggle_button_set_active (priv->panel_button, FALSE);
}

static void
set_search_section_visible (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gboolean allowable;
	gboolean visible;

	gchar **argv;
	gchar  *found_cmd = NULL;


	allowable = g_settings_get_boolean (priv->lockdown_settings, SEARCH_VIS_SETTINGS_KEY);

	if (allowable) {
		argv = get_search_argv (NULL);
		found_cmd = g_find_program_in_path (argv [0]);

		visible = (found_cmd != NULL);

		g_strfreev (argv);
		g_free (found_cmd);
	}
	else
		visible = FALSE;

	if (visible)
		gtk_widget_show (priv->search_section);
	else
		gtk_widget_hide (priv->search_section);
}

static void
set_table_section_visible (MainMenuUI *this, TileTable *table)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gint   table_id;
	GList *tiles;


	table_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (table), "table-id"));
	g_object_get (G_OBJECT (table), TILE_TABLE_TILES_PROP, & tiles, NULL);

	if (priv->allowable_types [table_id] && g_list_length (tiles) > 0)
		gtk_widget_show (priv->table_sections [table_id]);
	else
		gtk_widget_hide (priv->table_sections [table_id]);

	if (
		gtk_widget_get_visible (priv->table_sections [USER_APPS_TABLE]) ||
		gtk_widget_get_visible (priv->table_sections [RCNT_APPS_TABLE])
	)
		gtk_widget_show (priv->page_selectors [APPS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [APPS_PAGE]);

	if (
		gtk_widget_get_visible (priv->table_sections [USER_DOCS_TABLE]) ||
		gtk_widget_get_visible (priv->table_sections [RCNT_DOCS_TABLE])
	)
		gtk_widget_show (priv->page_selectors [DOCS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [DOCS_PAGE]);

	if (gtk_widget_get_visible (priv->table_sections [USER_DIRS_TABLE]))
		gtk_widget_show (priv->page_selectors [DIRS_PAGE]);
	else
		gtk_widget_hide (priv->page_selectors [DIRS_PAGE]);
}

static gchar **
get_search_argv (const gchar *search_txt)
{
	gchar  *cmd;
	gint    argc;
	gchar **argv_parsed = NULL;

	gchar **argv = NULL;

	gint i;

	GSettings *settings = NULL;

	settings = g_settings_new (SETTINGS_SCHEMA);

	cmd = g_settings_get_string (settings, SEARCH_CMD_SETTINGS_KEY);

	g_object_unref (settings);

	if (! cmd) {
		g_warning ("could not find search command in gsettings [" SEARCH_CMD_SETTINGS_KEY "]\n");

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

	MatePanelAppletOrient orientation;

	GtkWidget *child;


	orientation = mate_panel_applet_get_orient (priv->panel_applet);

	child = gtk_bin_get_child (GTK_BIN (priv->panel_applet));

	if (GTK_IS_WIDGET (child))
		gtk_container_remove (GTK_CONTAINER (priv->panel_applet), child);

	switch (orientation) {
		case MATE_PANEL_APPLET_ORIENT_LEFT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_RIGHT];
			break;

		case MATE_PANEL_APPLET_ORIENT_RIGHT:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_LEFT];
			break;

		case MATE_PANEL_APPLET_ORIENT_UP:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_BOTTOM];
			break;

		default:
			priv->panel_button = priv->panel_buttons [PANEL_BUTTON_ORIENT_TOP];
			break;
	}

	gtk_container_add (GTK_CONTAINER (priv->panel_applet), GTK_WIDGET (priv->panel_button));
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

	if (g_settings_get_boolean (priv->settings, URGENT_CLOSE_SETTINGS_KEY)) {
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
apply_lockdown_settings (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	gboolean    more_link_visible;
	gboolean    status_area_visible;
	gboolean    system_area_visible;
	GVariant   *showable_types_value;
	const gint *showable_types;
	gsize       nvalues;
	gint        i;

	libslab_checkpoint ("apply_lockdown_settings(): start");

	more_link_visible    = g_settings_get_boolean (priv->lockdown_settings, MORE_LINK_VIS_SETTINGS_KEY);
	status_area_visible  = g_settings_get_boolean (priv->lockdown_settings, STATUS_VIS_SETTINGS_KEY);
	system_area_visible  = g_settings_get_boolean (priv->lockdown_settings, SYSTEM_VIS_SETTINGS_KEY);
	showable_types_value = g_settings_get_value (priv->lockdown_settings, SHOWABLE_TYPES_SETTINGS_KEY);
	showable_types       = g_variant_get_fixed_array (showable_types_value, &nvalues, sizeof (gint32));

	for (i = 0; i < 3; ++i)
		if (more_link_visible)
			gtk_widget_show (priv->more_sections [i]);
		else
			gtk_widget_hide (priv->more_sections [i]);

	set_search_section_visible (this);

	if (status_area_visible)
		gtk_widget_show (priv->status_section);
	else
		gtk_widget_hide (priv->status_section);

	if (system_area_visible)
		gtk_widget_show (priv->system_section);
	else
		gtk_widget_hide (priv->system_section);

	for (i = 0; i < 5; ++i)
		priv->allowable_types [i] = FALSE;

	if (nvalues > 0) {
		for (i = 0; showable_types[i] != NULL; i++) {
			if (0 <= showable_types[i] && showable_types[i] < 5)
				priv->allowable_types [showable_types[i]] = TRUE;
		}
	}

	g_variant_unref (showable_types_value);

	for (i = 0; i < 5; ++i)
		set_table_section_visible (this, priv->file_tables [i]);

	libslab_checkpoint ("apply_lockdown_settings(): loading sys_table");
	tile_table_reload (priv->sys_table);

	libslab_checkpoint ("apply_lockdown_settings(): loading user_apps_table");
	tile_table_reload (priv->file_tables [USER_APPS_TABLE]);

	libslab_checkpoint ("apply_lockdown_settings(): loading user_docs_table");
	tile_table_reload (priv->file_tables [USER_DOCS_TABLE]);

	libslab_checkpoint ("apply_lockdown_settings(): loading user_dirs_table");
	tile_table_reload (priv->file_tables [USER_DIRS_TABLE]);

	libslab_checkpoint ("apply_lockdown_settings(): update_limits");
	update_limits (this);

	libslab_checkpoint ("apply_lockdown_settings(): end");
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

static GBookmarkFile *
load_recently_used_store (void)
{
	GBookmarkFile *store;
	char *filename;

	store = g_bookmark_file_new ();

	filename = get_recently_used_store_filename ();

	/* FIXME: if we can't load the store, do we need to hide the
	 * recently-used sections in the GUI?  If so, do that in the caller(s)
	 * of this function, not here.
	 */

	libslab_checkpoint ("main-menu-ui.c: load_recently_used_store(): start loading %s", filename);
	g_bookmark_file_load_from_file (store, filename, NULL); /* NULL-GError */
	libslab_checkpoint ("main-menu-ui.c: load_recently_used_store(): end loading %s", filename);

	g_free (filename);

	return store;
}

/* Updates the bookmark agents for the recently-used apps and documents, by reading the
 * recently-used store.
 */
static void
update_recently_used_bookmark_agents (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);
	GBookmarkFile *store;

	store = load_recently_used_store ();

	bookmark_agent_update_from_bookmark_file (priv->bm_agents[BOOKMARK_STORE_RECENT_APPS], store);
	bookmark_agent_update_from_bookmark_file (priv->bm_agents[BOOKMARK_STORE_RECENT_DOCS], store);

	g_bookmark_file_free (store);
}

/* Updates the recently-used tile tables from their corresponding bookmark agents */
static void
update_recently_used_tables (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	tile_table_reload (priv->file_tables[RCNT_APPS_TABLE]);
	tile_table_reload (priv->file_tables[RCNT_DOCS_TABLE]);
}

/* If the recently-used store has changed since the last time we updated from
 * it, this updates our view of the store and the corresponding sections in the
 * slab_window.
 */
static void
update_recently_used_sections (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

	libslab_checkpoint ("main-menu-ui.c: update_recently_used_sections() start");

	if (priv->recently_used_store_has_changed) {
		update_recently_used_bookmark_agents (this);
		update_recently_used_tables (this);

		priv->recently_used_store_has_changed = FALSE;
	}

	if (!priv->recently_used_store_monitor)
		setup_recently_used_store_monitor (this, FALSE);

	libslab_checkpoint ("main-menu-ui.c: update_recently_used_sections() end");
}

/* Updates the slab_window's sections that need updating and presents the window */
static void
present_slab_window (MainMenuUI *this)
{
	MainMenuUIPrivate *priv = PRIVATE (this);

#ifdef HAVE_NETWORK
	network_tile_update_status (priv->network_status);
#endif

	update_recently_used_sections (this);

	gtk_window_set_screen (GTK_WINDOW (priv->slab_window), gtk_widget_get_screen (GTK_WIDGET (priv->panel_applet)));
	gtk_window_present_with_time (GTK_WINDOW (priv->slab_window), gtk_get_current_event_time ());
}

static void
panel_button_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (button);

	DoubleClickDetector *detector;

	gboolean visible;

	main_menu_delayed_setup (this);

	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (toggle), "double-click-detector"));

	visible = gtk_widget_get_visible (priv->slab_window);

	if (! double_click_detector_is_double_click (detector, gtk_get_current_event_time (), TRUE)) {
		if (! visible)
			present_slab_window (this);
		else
			gtk_widget_hide (priv->slab_window);

		visible = gtk_widget_get_visible (priv->slab_window);
	}

	gtk_toggle_button_set_active (priv->panel_button, visible);
}

static gboolean
panel_button_button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	if (event->button != 1)
		g_signal_stop_emission_by_name (widget, "button_press_event");
	else if (! gtk_toggle_button_get_active (priv->panel_button))
		gtk_toggle_button_set_active (priv->panel_button, TRUE);
	else
		/* do nothing */ ;

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
			tile_table_add_uri (priv->file_tables [USER_APPS_TABLE], uris [i]);
		else
			tile_table_add_uri (priv->file_tables [USER_DOCS_TABLE], uris [i]);
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
		widget->style->bg [GTK_STATE_NORMAL].red   / 65535.0,
		widget->style->bg [GTK_STATE_NORMAL].green / 65535.0,
		widget->style->bg [GTK_STATE_NORMAL].blue  / 65535.0);

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

	cairo_pattern_destroy (gradient);
	cairo_destroy (cr);

	return FALSE;
}

static gboolean
slab_window_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	switch (event->keyval) {
		case GDK_Super_L:
		case GDK_Escape:
			gtk_toggle_button_set_active (priv->panel_button, FALSE);

			return TRUE;

		case GDK_W:
		case GDK_w:
			if (event->state & GDK_CONTROL_MASK) {
				gtk_toggle_button_set_active (priv->panel_button, FALSE);

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
	
	
	ptr_window = gdk_window_at_pointer (NULL, NULL);

	if (priv->slab_window->window != ptr_window) {
		hide_slab_if_urgent_close (this);

		return TRUE;
	}

	return FALSE;
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

	MatePanelAppletOrient orient;


	gdk_window_get_origin (GTK_WIDGET (priv->panel_button)->window, & button_geom.x, & button_geom.y);
	button_geom.width  = GTK_WIDGET (priv->panel_button)->allocation.width;
	button_geom.height = GTK_WIDGET (priv->panel_button)->allocation.height;

	slab_geom.width  = priv->slab_window->allocation.width;
	slab_geom.height = priv->slab_window->allocation.height;

	panel_button_screen = gtk_widget_get_screen (GTK_WIDGET (priv->panel_button));

	gdk_screen_get_monitor_geometry (
		panel_button_screen,
		gdk_screen_get_monitor_at_window (
			panel_button_screen, GTK_WIDGET (priv->panel_button)->window),
		& monitor_geom);

	orient = mate_panel_applet_get_orient (priv->panel_applet);

	switch (orient) {
		case MATE_PANEL_APPLET_ORIENT_UP:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y - slab_geom.height;
			break;

		case MATE_PANEL_APPLET_ORIENT_DOWN:
			slab_geom.x = button_geom.x;
			slab_geom.y = button_geom.y + button_geom.height;
			break;

		case MATE_PANEL_APPLET_ORIENT_RIGHT:
			slab_geom.x = button_geom.x + button_geom.width;
			slab_geom.y = button_geom.y;
			break;

		case MATE_PANEL_APPLET_ORIENT_LEFT:
			slab_geom.x = button_geom.x - slab_geom.width;
			slab_geom.y = button_geom.y;
			break;

		default:
			slab_geom.x = 0;
			slab_geom.y = 0;
			break;
	}

	if (orient == MATE_PANEL_APPLET_ORIENT_UP || orient == MATE_PANEL_APPLET_ORIENT_DOWN) {
		if ((slab_geom.x + slab_geom.width) > (monitor_geom.x + monitor_geom.width))
			slab_geom.x = MAX (monitor_geom.x, monitor_geom.x + monitor_geom.width - slab_geom.width);
	}
	else {
		if ((slab_geom.y + slab_geom.height) > (monitor_geom.y + monitor_geom.height))
			slab_geom.y = MAX (monitor_geom.y, monitor_geom.y + monitor_geom.height - slab_geom.height);
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
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	gint page_type;


	page_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "page-type"));

	gtk_notebook_set_current_page (priv->file_section, priv->notebook_page_ids [page_type]);

	g_settings_set_int (priv->filearea_settings, CURRENT_PAGE_SETTINGS_KEY, page_type);
}

static void
tile_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	gint table_id;


	connect_to_tile_triggers (this, TILE_TABLE (g_obj));

	table_id = GPOINTER_TO_INT (g_object_get_data (g_obj, "table-id"));

	switch (table_id) {
		case USER_APPS_TABLE:
			tile_table_reload (priv->file_tables [RCNT_APPS_TABLE]);
			break;

		case USER_DOCS_TABLE:
			tile_table_reload (priv->file_tables [RCNT_DOCS_TABLE]);
			break;

		default:
			break;
	}

	set_table_section_visible (this, TILE_TABLE (g_obj));

	update_limits (this);
}

static void
gtk_table_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	gint table_id = GPOINTER_TO_INT (g_object_get_data (g_obj, "table-id"));

	if (table_id == USER_APPS_TABLE || table_id == USER_DOCS_TABLE)
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
more_buttons_clicked_cb (GtkButton *button, gpointer user_data)
{
	MainMenuUI        *this = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	DoubleClickDetector *detector;
	GTimeVal current_time;
	guint32 current_time_millis;

	GError           *error = NULL;
	GDesktopAppInfo  *ditem;
	gchar            *ditem_id;

	gchar *cmd_template;
	gchar *cmd;
	gchar *dir;
	gchar *uri;


	detector = DOUBLE_CLICK_DETECTOR (
		g_object_get_data (G_OBJECT (button), "double-click-detector"));

	g_get_current_time (& current_time);

	current_time_millis = 1000 * current_time.tv_sec + current_time.tv_usec / 1000;

	if (! double_click_detector_is_double_click (detector, current_time_millis, TRUE)) {
		if (GTK_WIDGET (button) == priv->more_buttons [APPS_PAGE])
			ditem_id = g_settings_get_string (priv->settings, APP_BROWSER_SETTINGS_KEY);
		else if (GTK_WIDGET (button) == priv->more_buttons [DOCS_PAGE]) {
			dir = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));
			if (! dir)
			      dir = g_build_filename (g_get_home_dir (), "Documents", NULL);

			if (! g_file_test (dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
				g_free (dir);
				dir = g_strdup (g_get_home_dir ());
			}

			uri = g_filename_to_uri (dir, NULL, NULL);

			cmd_template = g_settings_get_string (priv->filearea_settings, FILE_MGR_OPEN_SETTINGS_KEY);
			cmd = libslab_string_replace_once (cmd_template, "FILE_URI", uri);

			libslab_spawn_command (cmd);

			g_free (cmd);
			g_free (cmd_template);
			g_free (uri);
			g_free (dir);

			ditem_id = NULL;

			hide_slab_if_urgent_close (this);
		}
		else
			ditem_id = g_settings_get_string (priv->settings, FILE_BROWSER_SETTINGS_KEY);

		ditem = g_desktop_app_info_new (ditem_id);

		if (ditem) {
			g_app_info_launch (G_APP_INFO (ditem), NULL, NULL, &error);
			if (error) {
				g_error_free (error);
			}
			g_object_unref (ditem);

			hide_slab_if_urgent_close (this);
		}
		if (ditem_id)
			g_free (ditem_id);
	}
}

static void
search_cmd_notify_cb (GSettings *settings, gchar *key, gpointer user_data)
{
	set_search_section_visible (MAIN_MENU_UI (user_data));
}

static void
current_page_notify_cb (GSettings *settings, gchar *key, gpointer user_data)
{
	select_page (MAIN_MENU_UI (user_data));
}

static void
lockdown_notify_cb (GSettings *settings, gchar *key, gpointer user_data)
{
	apply_lockdown_settings (MAIN_MENU_UI (user_data));
}

static void
panel_menu_open_cb (GtkAction *action, gpointer user_data)
{
	gtk_toggle_button_set_active (PRIVATE (user_data)->panel_button, TRUE);
}

static void
panel_menu_about_cb (GtkAction *action, gpointer user_data)
{
	MainMenuUI *this        = MAIN_MENU_UI (user_data);
	MainMenuUIPrivate *priv = PRIVATE      (this);

	main_menu_delayed_setup (this);

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
panel_applet_change_orient_cb (MatePanelApplet *applet, MatePanelAppletOrient orient, gpointer user_data)
{
	reorient_panel_button (MAIN_MENU_UI (user_data));
}

static void
panel_applet_change_background_cb (MatePanelApplet *applet, MatePanelAppletBackgroundType type, GdkColor *color,
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
	g_object_unref (rc_style);

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
	gtk_toggle_button_set_active (PRIVATE (user_data)->panel_button, TRUE);
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

static GdkFilterReturn
slab_gdk_message_filter (GdkXEvent *gdk_xevent, GdkEvent *event, gpointer user_data)
{
	MainMenuUIPrivate *priv = PRIVATE (user_data);

	XEvent *xevent = (XEvent *) gdk_xevent;
	gboolean active;


	if (xevent->type != ClientMessage)
		return GDK_FILTER_CONTINUE;

	if (xevent->xclient.data.l [0] == slab_action_main_menu_atom) {
		active = gtk_toggle_button_get_active (priv->panel_button);

		gtk_toggle_button_set_active (priv->panel_button, ! active);
	}
	else
		return GDK_FILTER_CONTINUE;

	return GDK_FILTER_REMOVE;
}

static void
user_app_agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	tile_table_reload (PRIVATE (user_data)->file_tables [RCNT_APPS_TABLE]);
}

static void
user_doc_agent_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer user_data)
{
	tile_table_reload (PRIVATE (user_data)->file_tables [RCNT_DOCS_TABLE]);
}

static void
volume_monitor_mount_cb (GVolumeMonitor *mon, GMount *mount, gpointer data)
{
	MainMenuUIPrivate *priv = PRIVATE (data);

	g_list_foreach (priv->mounts, (GFunc) g_object_unref, NULL);
	g_list_free (priv->mounts);
	priv->mounts = g_volume_monitor_get_mounts (mon);
}
