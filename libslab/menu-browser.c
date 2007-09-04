#include "menu-browser.h"

#include <glib/gi18n.h>
#include <glade/glade.h>
#include <libgnome/gnome-desktop-item.h>

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "application-tile.h"
#include "grid-view.h"
#include "libslab-utils.h"

typedef struct {
	GMenuTree    *menu_tree;
	GtkTreeStore *menu_store;

	GtkWidget    *view;

	GtkContainer *shortcuts_ctnr;
} MenuBrowserPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MENU_BROWSER_TYPE, MenuBrowserPrivate))

static void this_class_init (MenuBrowserClass *);
static void this_init       (MenuBrowser *);

static void finalize (GObject *);

static void load_tree        (MenuBrowser *);
static void get_dir_contents (GMenuTreeDirectory *, GList **);

static void clicked_cb (GtkButton *, gpointer);
static void changed_cb (GtkEditable *, gpointer);

static GtkAlignmentClass *this_parent_class;

GType
menu_browser_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			GTK_TYPE_ALIGNMENT, "MenuBrowser",
			sizeof (MenuBrowserClass), (GClassInitFunc) this_class_init,
			sizeof (MenuBrowser), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

GtkWidget *
menu_browser_new (GMenuTree *menu_tree)
{
	GtkWidget          *this;
	MenuBrowserPrivate *priv;

	GtkWidget *base_widget;
	GtkWidget *base_parent;

	gchar    *xml_path;
	GladeXML *xml;


	this = GTK_WIDGET (g_object_new (MENU_BROWSER_TYPE, NULL));
	priv = PRIVATE (this);

	xml_path = g_build_filename (DATADIR, PACKAGE, "menu-browser.glade", NULL);
	xml      = glade_xml_new (xml_path, "main-window", NULL);

	base_widget = glade_xml_get_widget (xml, "base-widget");
	base_parent = gtk_widget_get_parent (base_widget);

	gtk_widget_ref (base_widget);
	gtk_container_remove (GTK_CONTAINER (base_parent), base_widget);
	gtk_widget_destroy (base_parent);
	gtk_container_add (GTK_CONTAINER (this), base_widget);

	priv->menu_tree      = menu_tree;
	priv->menu_store     = gtk_tree_store_new (3, G_TYPE_STRING, GTK_TYPE_WIDGET, G_TYPE_POINTER);
	priv->shortcuts_ctnr = GTK_CONTAINER (glade_xml_get_widget (xml, "shortcuts-container"));
	priv->view           = grid_view_new_with_model (GTK_TREE_MODEL (priv->menu_store));

	gtk_container_add (GTK_CONTAINER (glade_xml_get_widget (xml, "browser-container")), priv->view);

	g_signal_connect (
		glade_xml_get_widget (xml, "filter-entry"),
		"changed", G_CALLBACK (changed_cb), this);

	load_tree (MENU_BROWSER (this));

	g_free (xml_path);
	g_object_unref (xml);

	return this;
}

static void
this_class_init (MenuBrowserClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (MenuBrowserPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (MenuBrowser *this)
{
	MenuBrowserPrivate *priv = PRIVATE (this);

	priv->menu_tree      = NULL;
	priv->menu_store     = NULL;

	priv->shortcuts_ctnr = NULL;
}

static void
finalize (GObject *g_obj)
{
	MenuBrowserPrivate *priv = PRIVATE (g_obj);

	gmenu_tree_unref (priv->menu_tree);

	if (G_IS_OBJECT (priv->menu_store))
		g_object_unref (priv->menu_store);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
load_tree (MenuBrowser *this)
{
	MenuBrowserPrivate *priv = PRIVATE (this);

	GMenuTreeDirectory *root;
	GSList             *contents = NULL;
	GMenuTreeItem      *item;

	GMenuTreeDirectory *dir;
	GList              *dir_list;
	const char         *cat_name;
	GtkWidget          *shortcut;
	Tile               *tile;
	gchar              *ditem_path;
	GnomeDesktopItem   *ditem;
	GList              *filter_strs;
	GtkTreeIter         iter_p;
	GtkTreeIter         iter_c;

	GSList             *node_menu;
	GList              *node_dir;


	if (! priv->menu_tree || ! priv->shortcuts_ctnr)
		return;

	root = gmenu_tree_get_root_directory (priv->menu_tree);
	g_return_if_fail (root);

	contents = gmenu_tree_directory_get_contents (root);
	g_return_if_fail (contents);

	for (node_menu = contents; node_menu; node_menu = node_menu->next) {
		item = (GMenuTreeItem *) node_menu->data;

		switch (gmenu_tree_item_get_type (item)) {
			case GMENU_TREE_ITEM_DIRECTORY:
				dir = (GMenuTreeDirectory *) item;

				cat_name = gmenu_tree_directory_get_name (dir);
				shortcut = gtk_button_new_with_label (cat_name);
				gtk_button_set_relief (GTK_BUTTON (shortcut), GTK_RELIEF_NONE);
				gtk_button_set_alignment (GTK_BUTTON (shortcut), 0.0, 0.5);
				gtk_container_add (priv->shortcuts_ctnr, shortcut);
				g_signal_connect (shortcut, "clicked", G_CALLBACK (clicked_cb), this);

				gtk_tree_store_append (priv->menu_store, & iter_p, NULL);
				gtk_tree_store_set (
					priv->menu_store, & iter_p, 0, cat_name, 1, NULL, 2, NULL, -1);

				dir_list = NULL;
				get_dir_contents (dir, & dir_list);

				for (node_dir = dir_list; node_dir; node_dir = node_dir->next) {
					ditem_path = (gchar *) node_dir->data;
					ditem      = libslab_gnome_desktop_item_new_from_unknown_id (ditem_path);

					tile = TILE (application_tile_new (ditem_path));

					filter_strs = NULL;
					filter_strs = g_list_append (
						filter_strs, g_strdup (gnome_desktop_item_get_string (
							ditem, GNOME_DESKTOP_ITEM_NAME)));
					filter_strs = g_list_append (
						filter_strs, g_strdup (gnome_desktop_item_get_string (
							ditem, GNOME_DESKTOP_ITEM_GENERIC_NAME)));
					filter_strs = g_list_append (
						filter_strs, g_strdup (gnome_desktop_item_get_string (
							ditem, GNOME_DESKTOP_ITEM_COMMENT)));
					filter_strs = g_list_append (
						filter_strs, g_strdup (gnome_desktop_item_get_string (
							ditem, GNOME_DESKTOP_ITEM_EXEC)));

					gtk_tree_store_append (priv->menu_store, & iter_c, & iter_p);
					gtk_tree_store_set (
						priv->menu_store, & iter_c,
						0, NULL, 1, tile_get_widget (tile), 2, filter_strs,
						-1);

					g_free (ditem_path);
					gnome_desktop_item_unref (ditem);
				}
				g_list_free (dir_list);

				break;

			default:
				break;
		}

		gmenu_tree_item_unref (item);
	}

	g_slist_free (contents);
}

static void
get_dir_contents (GMenuTreeDirectory *dir, GList **list)
{
	GSList *contents = NULL;
	GMenuTreeItem *item;

	GSList *node;


	if (! dir)
		return;

	contents = gmenu_tree_directory_get_contents (dir);

	for (node = contents; node; node = node->next) {
		item = (GMenuTreeItem *) node->data;

		switch (gmenu_tree_item_get_type (item)) {
			case GMENU_TREE_ITEM_DIRECTORY:
				get_dir_contents ((GMenuTreeDirectory *) item, list);
				break;

			case GMENU_TREE_ITEM_ENTRY:
				* list = g_list_append (* list, g_strdup (
					gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry *) item)));
				break;

			default:
				break;
		}

		gmenu_tree_item_unref (item);
	}

	g_slist_free (contents);
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	grid_view_scroll_to_node (GRID_VIEW (PRIVATE (data)->view), gtk_button_get_label (button));
}

static void
changed_cb (GtkEditable *editable, gpointer data)
{
	gchar *text = gtk_editable_get_chars (editable, 0, -1);
	
	grid_view_filter_nodes (GRID_VIEW (PRIVATE (data)->view), text);

	g_free (text);
}
