#include "storage-tile.h"

#include <glib/gi18n.h>

#include "storage-tile-model.h"
#include "tile-button-view.h"
#include "tile-attribute.h"
#include "tile-control.h"

typedef struct {
	StorageTileModel *model;
	TileButtonView   *view;

	TileControl      *info_control;
} StorageTilePrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), STORAGE_TILE_TYPE, StorageTilePrivate))

static void this_class_init (StorageTileClass *);
static void this_init       (StorageTile *);

static void       finalize   (GObject *);
static GtkWidget *get_widget (Tile *);
static gboolean   equals     (Tile *, gconstpointer);

static gchar *size_bytes_to_string (guint64);

static void clicked_cb    (GtkButton *, gpointer);
static void hide_event_cb (GtkWidget *, gpointer);
static void show_event_cb (GtkWidget *, gpointer);

static void info_trigger (TileAttribute *, TileAttribute *, gpointer);

static TileClass *this_parent_class = NULL;

GType
storage_tile_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_TYPE, "StorageTile",
			sizeof (StorageTileClass), (GClassInitFunc) this_class_init,
			sizeof (StorageTile), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

StorageTile *
storage_tile_new ()
{
	StorageTile        *this;
	StorageTilePrivate *priv;


	this = g_object_new (STORAGE_TILE_TYPE, NULL);
	priv = PRIVATE (this);

	priv->model = storage_tile_model_new ();
	priv->view  = tile_button_view_new (2);
	g_object_set (
		G_OBJECT (priv->view->icon), "icon-size", GTK_ICON_SIZE_BUTTON, NULL);

	tile_attribute_set_string (
		tile_button_view_get_icon_id_attr (priv->view), "gnome-dev-harddisk");
	tile_attribute_set_string (
		tile_button_view_get_header_text_attr (priv->view, 0), _("Hard Drive"));

	priv->info_control = tile_control_new_with_trigger_func (
		storage_tile_model_get_info_attr (priv->model),
		tile_button_view_get_header_text_attr (priv->view, 1),
		info_trigger, NULL);

	g_signal_connect (priv->view, "clicked", G_CALLBACK (clicked_cb),    this);
	g_signal_connect (priv->view, "hide",    G_CALLBACK (hide_event_cb), this);
	g_signal_connect (priv->view, "show",    G_CALLBACK (show_event_cb), this);

	return this;
}

static void
this_class_init (StorageTileClass *this_class)
{
	TileClass *tile_class = TILE_CLASS (this_class);

	G_OBJECT_CLASS (this_class)->finalize = finalize;

	tile_class->get_widget = get_widget;
	tile_class->equals     = equals;

	g_type_class_add_private (this_class, sizeof (StorageTilePrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (StorageTile *this)
{
	StorageTilePrivate *priv = PRIVATE (this);

	priv->model        = NULL;
	priv->view         = NULL;

	priv->info_control = NULL;
}

static void
finalize (GObject *g_obj)
{
	StorageTilePrivate *priv = PRIVATE (g_obj);

	g_object_unref (priv->model);

	if (G_IS_OBJECT (priv->view))
		g_object_unref (priv->view);

	g_object_unref (priv->info_control);

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
	return IS_STORAGE_TILE (that);
}

#define GIGA (1024 * 1024 * 1024)
#define MEGA (1024 * 1024)
#define KILO (1024)

static gchar *
size_bytes_to_string (guint64 size)
{
	gchar              *size_string;
	unsigned long long  size_bytes;


/* FIXME:  this is just a work around for gcc warnings about %lu not big enough
 * to hold guint64 on 32bit machines.  on 64bit machines, however, gcc warns
 * that %llu is too big for guint64.  ho-hum.
 */

	size_bytes = (unsigned long long) size;

	if (size_bytes > GIGA)
		size_string = g_strdup_printf (_("%lluG"), size_bytes / GIGA);
	else if (size_bytes > MEGA)
		size_string = g_strdup_printf (_("%lluM"), size_bytes / MEGA);
	else if (size_bytes > KILO)
		size_string = g_strdup_printf (_("%lluK"), size_bytes / KILO);
	else
		size_string = g_strdup_printf (_("%llub"), size_bytes);

	return size_string;
}

static void
clicked_cb (GtkButton *button, gpointer data)
{
	storage_tile_model_open (PRIVATE (data)->model);
	tile_action_triggered (TILE (data), TILE_ACTION_LAUNCHES_APP);
}

static void
hide_event_cb (GtkWidget *widget, gpointer data)
{
	storage_tile_model_stop_poll (PRIVATE (data)->model);
}

static void
show_event_cb (GtkWidget *widget, gpointer data)
{
	storage_tile_model_start_poll (PRIVATE (data)->model);
}

static void
info_trigger (TileAttribute *src, TileAttribute *dst, gpointer data)
{
	StorageTileModelStorageInfo *info;

	gchar *text;
	gchar *available;
	gchar *capacity;


	info = (StorageTileModelStorageInfo *) g_value_get_pointer (
		tile_attribute_get_value (src));

	if (! info)
		return;

	available = size_bytes_to_string (info->available_bytes);
	capacity  = size_bytes_to_string (info->capacity_bytes);
	text = g_strdup_printf (_("%s Free / %s Total"), available, capacity);

	tile_attribute_set_string (dst, text);

	g_free (available);
	g_free (capacity);
	g_free (text);
}
