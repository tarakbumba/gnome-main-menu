#include "document-tile.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "file-tile-model.h"
#include "tile-button-view.h"
#include "context-menu-view.h"
#include "tile-attribute.h"
#include "tile-control.h"
#include "libslab-utils.h"

typedef struct {
	FileTileModel   *model;
	TileButtonView  *view;
	ContextMenuView *menu;

	TileControl *icon_control;
	TileControl *name_hdr_control;
	TileControl *mtime_hdr_control;

	TileControl *open_menu_item_control;
} DocumentTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DOCUMENT_TILE_TYPE, DocumentTilePrivate))

static void this_class_init (DocumentTileClass *);
static void this_init       (DocumentTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void     clicked_cb                  (GtkButton *, gpointer);
static gboolean button_release_cb           (GtkWidget *, GdkEventButton *, gpointer);
static void     open_item_activate_cb       (GtkMenuItem *, gpointer);
static void     open_in_fb_item_activate_cb (GtkMenuItem *, gpointer);

static void map_mtime_to_string  (const GValue *, GValue *, gpointer);
static void map_app_to_menu_item (const GValue *, GValue *, gpointer);

static TileClass *this_parent_class = NULL;

GType
document_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "DocumentTile",
			sizeof (DocumentTileClass), (GClassInitFunc) this_class_init,
			sizeof (DocumentTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

DocumentTile *
document_tile_new (const gchar *uri)
{
	DocumentTile        *this;
	DocumentTilePrivate *priv;

	GtkWidget     *menu_item;
	TileAttribute *menu_attr;


	this = g_object_new (DOCUMENT_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = file_tile_model_new (uri);
	priv->view  = tile_button_view_new (2);
	priv->menu  = context_menu_view_new ();

	priv->icon_control = tile_control_new (
		file_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view),
		NULL, NULL);
	priv->name_hdr_control = tile_control_new (
		file_tile_model_get_file_name_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 0),
		NULL, NULL);
	priv->mtime_hdr_control = tile_control_new (
		file_tile_model_get_mtime_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 1),
		map_mtime_to_string, NULL);

/* make open in default app menu-item/attr */

	menu_attr = context_menu_view_add_menu_item (priv->menu, & menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_item_activate_cb), this);

	priv->open_menu_item_control = tile_control_new (
		file_tile_model_get_app_attr (priv->model), menu_attr,
		map_app_to_menu_item, NULL);

/* make open in file browser menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Open in File Manager"));
	gtk_menu_append (GTK_MENU (priv->menu), menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_in_fb_item_activate_cb), this);

	gtk_widget_show_all (GTK_WIDGET (priv->menu));

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);
	g_signal_connect (priv->view, "button-release-event", G_CALLBACK (button_release_cb), this);

	return this;
}

static void
this_class_init (DocumentTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (DocumentTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (DocumentTile *this)
{
	DocumentTilePrivate *priv = PRIVATE (this);

	priv->model                  = NULL;
	priv->view                   = NULL;
	priv->menu                   = NULL;
	priv->icon_control           = NULL;
	priv->name_hdr_control       = NULL;
	priv->mtime_hdr_control      = NULL;
	priv->open_menu_item_control = NULL;
}

static void
finalize (GObject *g_obj)
{
	DocumentTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);
	g_object_unref (priv->icon_control);
	g_object_unref (priv->name_hdr_control);
	g_object_unref (priv->mtime_hdr_control);
	g_object_unref (priv->open_menu_item_control);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	if (priv->menu)
		gtk_object_sink (GTK_OBJECT (priv->menu));

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

	if (IS_DOCUMENT_TILE (that))
		uri_that = tile_model_get_uri (TILE_MODEL (PRIVATE (that)->model));
	else
		uri_that = (const gchar *) that;

	return libslab_strcmp (uri_this, uri_that) == 0;
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	file_tile_model_open (PRIVATE (data)->model);

	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static gboolean
button_release_cb (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	DocumentTilePrivate *priv = PRIVATE (user_data);
	
	if (event->button == 3 && GTK_IS_MENU (priv->menu)) {
		gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}

	return FALSE;
}

static void
open_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_open (PRIVATE (data)->model);
}

static void
open_in_fb_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_open_in_file_browser (PRIVATE (data)->model);
}

static void
map_mtime_to_string (const GValue *val_mtime, GValue *val_string, gpointer data)
{
	GDate *time;
	gchar *time_str;
	
	
	time = g_date_new ();
	g_date_set_time (time, g_value_get_long (val_mtime));

	time_str = g_new0 (gchar, 256);

	g_date_strftime (time_str, 256, _("Edited %m/%d/%Y"), time);
	g_date_free (time);

	g_value_set_string (val_string, time_str);
}

static void
map_app_to_menu_item (const GValue *val_app, GValue *val_string, gpointer data)
{
	GnomeVFSMimeApplication *app = (GnomeVFSMimeApplication *) g_value_get_pointer (val_app);

	if (app)
		g_value_set_string (
			val_string, g_strdup_printf (_("<b>Open with %s</b>"), app->name));
	else
		g_value_set_string (val_string, _("Open with Default Application"));
}
