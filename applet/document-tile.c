#include "document-tile.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include "file-tile-model.h"
#include "tile-button-view.h"
#include "context-menu-view.h"
#include "tile-attribute.h"
#include "tile-control.h"
#include "libslab-utils.h"
#include "bookmark-agent.h"

typedef struct {
	FileTileModel  *model;
	TileButtonView *view;

	TileControl *uri_control;
	TileControl *icon_control;
	TileControl *name_hdr_control;
	TileControl *mtime_hdr_control;
	TileControl *tooltip_control;

	TileControl *open_menu_item_control;
	TileControl *send_to_menu_item_control;
	TileControl *is_in_store_control;
	TileControl *store_status_control;
	TileControl *can_delete_control;
} DocumentTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DOCUMENT_TILE_TYPE, DocumentTilePrivate))

static void this_class_init (DocumentTileClass *);
static void this_init       (DocumentTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static void clicked_cb                  (GtkButton *, gpointer);
static void open_item_activate_cb       (GtkMenuItem *, gpointer);
static void open_in_fb_item_activate_cb (GtkMenuItem *, gpointer);
static void rename_item_activate_cb     (GtkMenuItem *, gpointer);
static void send_to_item_activate_cb    (GtkMenuItem *, gpointer);
static void user_store_item_activate_cb (GtkMenuItem *, gpointer);
static void trash_item_activate_cb      (GtkMenuItem *, gpointer);
static void delete_item_activate_cb     (GtkMenuItem *, gpointer);
static void view_name_attr_notify_cb    (GObject *, GParamSpec *, gpointer);

static void mtime_trigger        (TileAttribute *, TileAttribute *, gpointer);
static void filename_trigger     (TileAttribute *, TileAttribute *, gpointer);
static void tooltip_trigger      (TileAttribute *, TileAttribute *, gpointer);
static void app_trigger          (TileAttribute *, TileAttribute *, gpointer);
static void send_to_trigger      (TileAttribute *, TileAttribute *, gpointer);
static void is_in_store_trigger  (TileAttribute *, TileAttribute *, gpointer);
static void store_status_trigger (TileAttribute *, TileAttribute *, gpointer);
static void can_delete_trigger   (TileAttribute *, TileAttribute *, gpointer);

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

	ContextMenuView *menu;

	GtkWidget     *menu_item;
	TileAttribute *menu_attr;


	this = g_object_new (DOCUMENT_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = file_tile_model_new (uri);
	priv->view  = tile_button_view_new (2);

	menu = context_menu_view_new ();

	tile_button_view_add_context_menu (priv->view, GTK_MENU (menu));

	priv->uri_control = tile_control_new (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_uri_attr (priv->view));
	priv->icon_control = tile_control_new (
		file_tile_model_get_icon_id_attr (priv->model),
		tile_button_view_get_icon_id_attr (priv->view));
	priv->name_hdr_control = tile_control_new_with_trigger_func (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_header_text_attr (priv->view, 0),
		filename_trigger, NULL);
	priv->mtime_hdr_control = tile_control_new_with_trigger_func (
		file_tile_model_get_mtime_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 1),
		mtime_trigger, NULL);
	priv->tooltip_control = tile_control_new_with_trigger_func (
		tile_model_get_uri_attr (TILE_MODEL (priv->model)),
		tile_button_view_get_tooltip_attr (priv->view),
		tooltip_trigger, NULL);

/* make open in default app menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_item_activate_cb), this);

	priv->open_menu_item_control = tile_control_new_with_trigger_func (
		file_tile_model_get_app_attr (priv->model), menu_attr, app_trigger, NULL);

/* make open in file browser menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Open in File Manager"));
	gtk_menu_append (GTK_MENU (menu), menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (open_in_fb_item_activate_cb), this);

/* insert separator */

	gtk_menu_append (GTK_MENU (menu), gtk_separator_menu_item_new ());

/* make rename menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Rename..."));
	gtk_menu_append (GTK_MENU (menu), menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (rename_item_activate_cb), this);

/* make send-to menu-item */

	menu_item = gtk_menu_item_new_with_label (_("Send To..."));
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (send_to_item_activate_cb), this);

	priv->send_to_menu_item_control = tile_control_new_with_trigger_func (
		file_tile_model_get_is_local_attr (priv->model), menu_attr,
		send_to_trigger, NULL);

/* make add/remove from favorites menu-item */

	menu_item = gtk_menu_item_new ();
	menu_attr = context_menu_view_add_menu_item (menu, menu_item);
	g_signal_connect (menu_item, "activate", G_CALLBACK (user_store_item_activate_cb), this);

	priv->is_in_store_control = tile_control_new_with_trigger_func (
		file_tile_model_get_is_in_store_attr (priv->model), menu_attr,
		is_in_store_trigger, NULL);
	priv->store_status_control = tile_control_new_with_trigger_func (
		file_tile_model_get_store_status_attr (priv->model), menu_attr,
		store_status_trigger, NULL);

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
		file_tile_model_get_can_delete_attr (priv->model), menu_attr,
		can_delete_trigger, NULL);

	gtk_widget_show_all (GTK_WIDGET (menu));

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb), this);

	g_signal_connect (
		tile_button_view_get_header_text_attr (priv->view, 0), 
		"notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (view_name_attr_notify_cb), this);

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

	priv->model                     = NULL;
	priv->view                      = NULL;

	priv->uri_control               = NULL;
	priv->icon_control              = NULL;
	priv->name_hdr_control          = NULL;
	priv->mtime_hdr_control         = NULL;
	priv->tooltip_control           = NULL;

	priv->open_menu_item_control    = NULL;
	priv->send_to_menu_item_control = NULL;
	priv->is_in_store_control       = NULL;
	priv->store_status_control      = NULL;
	priv->can_delete_control        = NULL;
}

static void
finalize (GObject *g_obj)
{
	DocumentTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	g_object_unref (priv->uri_control);
	g_object_unref (priv->icon_control);
	g_object_unref (priv->name_hdr_control);
	g_object_unref (priv->mtime_hdr_control);
	g_object_unref (priv->tooltip_control);
	g_object_unref (priv->open_menu_item_control);
	g_object_unref (priv->send_to_menu_item_control);
	g_object_unref (priv->is_in_store_control);
	g_object_unref (priv->store_status_control);
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

static void
open_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_open (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
open_in_fb_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_open_in_file_browser (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
rename_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	tile_button_view_activate_header_edit (PRIVATE (data)->view, 0);
}

static void
send_to_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_send_to (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
user_store_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_user_store_toggle (PRIVATE (data)->model);
}

static void
trash_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_trash (PRIVATE (data)->model);
}

static void
delete_item_activate_cb (GtkMenuItem *menu_item, gpointer data)
{
	file_tile_model_delete (PRIVATE (data)->model);
}

static void
view_name_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	GValue *val = tile_attribute_get_value (TILE_ATTRIBUTE (g_obj));

	file_tile_model_rename (PRIVATE (data)->model, g_value_get_string (val));
}

static void
mtime_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	GDate *time;
	gchar *time_str;


	time = g_date_new ();
	g_date_set_time (time, g_value_get_long (tile_attribute_get_value (src)));

	time_str = g_new0 (gchar, 256);

	g_date_strftime (time_str, 256, _("Edited %m/%d/%Y"), time);
	g_date_free (time);

	tile_attribute_set_string (dst, time_str);
	g_free (time_str);
}

static void
filename_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	gchar *basename;
	gchar *filename;


	basename = g_path_get_basename (g_value_get_string (tile_attribute_get_value (src)));
	filename = gnome_vfs_unescape_string (basename, NULL);

	tile_attribute_set_string (dst, filename);

	g_free (basename);
	g_free (filename);
}

static void
tooltip_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	gchar *path;

	path = g_filename_from_uri (g_value_get_string (tile_attribute_get_value (src)), NULL, NULL);

	tile_attribute_set_string (dst, path);

	g_free (path);
}

static void
app_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	GnomeVFSMimeApplication *app;
	gchar                   *menu_label;

	app = (GnomeVFSMimeApplication *) g_value_get_pointer (tile_attribute_get_value (src));

	if (app)
		menu_label = g_strdup_printf (_("<b>Open with %s</b>"), app->name);
	else
		menu_label = g_strdup (_("Open with Default Application"));

	tile_attribute_set_string (dst, menu_label);
	g_free (menu_label);
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
can_delete_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	if (g_value_get_boolean (tile_attribute_get_value (src)))
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_ACTIVE);
	else
		tile_attribute_set_status (dst, TILE_ATTRIBUTE_HIDDEN);
}
