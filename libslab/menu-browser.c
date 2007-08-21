#include "menu-browser.h"

#include <glib/gi18n.h>
#include <glade/glade.h>

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "libslab-utils.h"

typedef struct {
	GtkContainer *shortcuts_ctnr;
	GtkContainer *browser_ctnr;
} MenuBrowserPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MENU_BROWSER_TYPE, MenuBrowserPrivate))

static void this_class_init (MenuBrowserClass *);
static void this_init       (MenuBrowser *);

static GObject *constructor  (GType, guint, GObjectConstructParam *);
static void     get_property (GObject *, guint, GValue *, GParamSpec *);
static void     set_property (GObject *, guint, const GValue *, GParamSpec *);
static void     finalize     (GObject *);

static void load_tree        (MenuBrowser *, GMenuTree *);
static void get_dir_contents (GList **, GMenuTreeDirectory *);

enum {
	PROP_0,
	PROP_MTREE
};

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

static void
this_class_init (MenuBrowserClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *mtree_pspec;


	g_obj_class->constructor  = constructor;
	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;
	g_obj_class->finalize     = finalize;

	mtree_pspec = g_param_spec_pointer (
		MENU_BROWSER_MENU_TREE_PROP, MENU_BROWSER_MENU_TREE_PROP,
		"a pointer to a GMenuTree object to browse",
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT |
		G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_MTREE, mtree_pspec);

	g_type_class_add_private (this_class, sizeof (MenuBrowserPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (MenuBrowser *this)
{
	MenuBrowserPrivate *priv = PRIVATE (this);

	priv->shortcuts_ctnr = NULL;
	priv->browser_ctnr   = NULL;
}

static GObject *
constructor (GType type, guint n_props, GObjectConstructParam *props)
{
	GObject            *this;
	MenuBrowserPrivate *priv;

	GtkWidget *base_widget;
	GtkWidget *base_parent;

	gchar    *xml_path;
	GladeXML *xml;

	GMenuTree *tree = NULL;

	gint i;


	this = G_OBJECT_CLASS (this_parent_class)->constructor (type, n_props, props);
	priv = PRIVATE (this);

	xml_path = g_build_filename (DATADIR, PACKAGE, "menu-browser.glade", NULL);
	xml      = glade_xml_new (xml_path, "main-window", NULL);

	base_widget = glade_xml_get_widget (xml, "base-widget");
	base_parent = gtk_widget_get_parent (base_widget);

	gtk_widget_ref (base_widget);
	gtk_container_remove (GTK_CONTAINER (base_parent), base_widget);
	gtk_widget_destroy (base_parent);
	gtk_container_add (GTK_CONTAINER (this), base_widget);

	priv->shortcuts_ctnr = GTK_CONTAINER (glade_xml_get_widget (xml, "shortcuts-container"));
	priv->browser_ctnr   = GTK_CONTAINER (glade_xml_get_widget (xml, "browser-container"));

	for (i = 0; tree == NULL && i < n_props; ++i)
		if (
			props [i].pspec->owner_type == MENU_BROWSER_TYPE &&
			! libslab_strcmp (props [i].pspec->name, MENU_BROWSER_MENU_TREE_PROP)
		)
			tree = (GMenuTree *) g_value_get_pointer (props [i].value);

	load_tree (MENU_BROWSER (this), tree);

	g_free (xml_path);
	g_object_unref (xml);

	return this;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	/* no readable properties */
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	if (prop_id == PROP_MTREE)
		load_tree (MENU_BROWSER (g_obj), (GMenuTree *) g_value_get_pointer (value));
}

static void
finalize (GObject *g_obj)
{
	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
load_tree (MenuBrowser *this, GMenuTree *tree)
{
	MenuBrowserPrivate *priv = PRIVATE (this);

	GMenuTreeDirectory *root;
	GSList             *contents = NULL;
	GMenuTreeItem      *item;

	GMenuTreeDirectory *dir;
	GMenuTreeEntry     *entry;
	GtkListStore       *cat_store;
	GtkWidget          *cat_view;
	GtkTreeIter         iter;
	const char         *cat_name;
	GtkWidget          *shortcut;
	GList              *dir_list;
	GList              *misc_list = NULL;
	GdkPixbuf          *icon;

	GSList *snode;
	GList  *node;


	if (! tree || ! priv->shortcuts_ctnr)
		return;

	root = gmenu_tree_get_root_directory (tree);

	if (! root)
		return;

	contents = gmenu_tree_directory_get_contents (root);

	for (snode = contents; snode; snode = snode->next) {
		item = (GMenuTreeItem *) snode->data;

		switch (gmenu_tree_item_get_type (item)) {
			case GMENU_TREE_ITEM_DIRECTORY:
				dir = (GMenuTreeDirectory *) item;

				cat_name = gmenu_tree_directory_get_name (dir);
				shortcut = gtk_button_new_with_label (cat_name);
				gtk_button_set_relief (GTK_BUTTON (shortcut), GTK_RELIEF_NONE);
				gtk_container_add (priv->shortcuts_ctnr, shortcut);

				dir_list = NULL;
				get_dir_contents (& dir_list, dir);

				cat_store = gtk_list_store_new (1, GDK_TYPE_PIXBUF);

				for (node = dir_list; node; node = node->next) {
					icon = gtk_icon_theme_load_icon (
						gtk_icon_theme_get_default (), (gchar *) node->data,
						GTK_ICON_SIZE_DND, 0, NULL);

					gtk_list_store_append (cat_store, & iter);
					gtk_list_store_set    (cat_store, & iter, 0, icon, -1);

					g_free (node->data);
				}
				g_list_free (dir_list);

				cat_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (cat_store));
				gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (cat_view), 0);

				gtk_container_add (priv->browser_ctnr, gtk_label_new (cat_name));
				gtk_container_add (priv->browser_ctnr, cat_view);

				break;

			case GMENU_TREE_ITEM_ENTRY:
				entry = (GMenuTreeEntry *) item;

				misc_list = g_list_append (misc_list, g_strdup (gmenu_tree_entry_get_icon (entry)));

				break;

			default:
				break;
		}

		gmenu_tree_item_unref (item);
	}

	g_slist_free (contents);

	if (misc_list) {
		for (node = misc_list; node; node = node->next) {
			icon = gtk_icon_theme_load_icon (
				gtk_icon_theme_get_default (), (gchar *) node->data,
				GTK_ICON_SIZE_DIALOG, 0, NULL);

			gtk_list_store_append (cat_store, & iter);
			gtk_list_store_set    (cat_store, & iter, 0, icon, -1);

			g_free (node->data);
		}
		g_list_free (misc_list);

		cat_view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (cat_store));
		gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (cat_view), 0);

		gtk_container_add (priv->browser_ctnr, gtk_label_new (_("Other")));
		gtk_container_add (priv->browser_ctnr, cat_view);
	}
}

static void
get_dir_contents (GList **list, GMenuTreeDirectory *dir)
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
				get_dir_contents (list, (GMenuTreeDirectory *) item);
				break;

			case GMENU_TREE_ITEM_ENTRY:
				* list = g_list_append (* list, g_strdup (
					gmenu_tree_entry_get_icon ((GMenuTreeEntry *) item)));
				break;

			default:
				break;
		}

		gmenu_tree_item_unref (item);
	}

	g_slist_free (contents);
}
