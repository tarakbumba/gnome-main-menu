#include "storage-tile-model.h"

#include <string.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib/gi18n.h>
#include <glibtop/fsusage.h>
#include <libhal.h>
#include <libhal-storage.h>
#include <gconf/gconf-client.h>

#include "libslab-utils.h"

typedef struct {
	TileAttribute               *info_attr;

	StorageTileModelStorageInfo *info;
	LibHalContext               *hal_context;
	gint                         timeout_millis;

	guint                        timeout_monitor_id;
	guint                        poll_handle;
} StorageTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), STORAGE_TILE_MODEL_TYPE, StorageTileModelPrivate))

#define TIMEOUT_KEY_DIR "/apps/procman"
#define TIMEOUT_KEY     "/apps/procman/disks_interval"

static void this_class_init (StorageTileModelClass *);
static void this_init       (StorageTileModel *);

static void finalize (GObject *);

static void update_model     (StorageTileModel *);
static void update_timeout   (StorageTileModel *);
static void init_hal_context (StorageTileModel *);

static gboolean poll_func (gpointer);

static void timeout_notify_cb (GConfClient *, guint, GConfEntry *, gpointer);

static GObjectClass *this_parent_class = NULL;

GType
storage_tile_model_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_MODEL_TYPE, "StorageTileModel",
			sizeof (StorageTileModelClass), (GClassInitFunc) this_class_init,
			sizeof (StorageTileModel), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

StorageTileModel *
storage_tile_model_new ()
{
	StorageTileModel        *this;
	StorageTileModelPrivate *priv;


	this = g_object_new (STORAGE_TILE_MODEL_TYPE, NULL);
	priv = PRIVATE (this);

	priv->info_attr = tile_attribute_new (G_TYPE_POINTER);
	priv->info      = g_new0 (StorageTileModelStorageInfo, 1);

	update_timeout (this);

	priv->timeout_monitor_id = libslab_gconf_notify_add (
		TIMEOUT_KEY, timeout_notify_cb, this);

	return this;
}

TileAttribute *
storage_tile_model_get_info_attr (StorageTileModel *this)
{
	return PRIVATE (this)->info_attr;
}

void
storage_tile_model_open (StorageTileModel *this)
{
}

void
storage_tile_model_start_poll (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	update_model (this);

	if (! priv->poll_handle)
		priv->poll_handle = g_timeout_add (priv->timeout_millis, poll_func, this);
}

void
storage_tile_model_stop_poll (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	if (priv->poll_handle) {
		g_source_remove (priv->poll_handle);
		priv->poll_handle = 0;
	}
}

static void
this_class_init (StorageTileModelClass *this_class)
{
	G_OBJECT_CLASS (this_class)->finalize = finalize;

	g_type_class_add_private (this_class, sizeof (StorageTileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	GConfClient *client;


	priv->info               = NULL;
	priv->info_attr          = NULL;

	priv->hal_context        = NULL;
	priv->timeout_millis     = 0;

	priv->timeout_monitor_id = 0;
	priv->poll_handle        = 0;

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, TIMEOUT_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	g_object_unref (client);
}

static void
finalize (GObject *g_obj)
{
	StorageTileModelPrivate *priv = PRIVATE (g_obj);

	GConfClient *client;


	g_free (priv->info);
	g_object_unref (priv->info_attr);

	libslab_gconf_notify_remove (priv->timeout_monitor_id);

	if (priv->poll_handle)
		g_source_remove (priv->poll_handle);

	client = gconf_client_get_default ();
	gconf_client_remove_dir (client, TIMEOUT_KEY_DIR, NULL);
	g_object_unref (client);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
update_model (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	gchar        **udis;
	LibHalVolume  *vol;
	gint           n_udis;

	glibtop_fsusage fs_usage;

	DBusError error;

	gint i;


	if (! priv->hal_context)
		init_hal_context (this);

	if (! priv->hal_context)
		return;

	dbus_error_init (& error);
	udis = libhal_find_device_by_capability (
		priv->hal_context, "volume", & n_udis, & error);

	if (dbus_error_is_set (& error)) {
		g_warning ("%s: error (%s): [%s]\n", G_STRFUNC, error.name, error.message);

		n_udis = 0;
	}

	dbus_error_free (& error);

	for (i = 0; i < n_udis; ++i) {
		vol = libhal_volume_from_udi (priv->hal_context, udis [i]);

		if (libhal_volume_is_mounted (vol) && ! libhal_volume_is_disc (vol)) {
			glibtop_get_fsusage (
				& fs_usage, libhal_volume_get_mount_point (vol));

			priv->info->available_bytes += fs_usage.bfree  * fs_usage.block_size;
			priv->info->capacity_bytes  += fs_usage.blocks * fs_usage.block_size;
		}

		libhal_volume_free (vol);
	}

	libhal_free_string_array (udis);
}

static void
update_timeout (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	gint t_new;


	t_new = MAX (GPOINTER_TO_INT (libslab_get_gconf_value (TIMEOUT_KEY)), 1000);

	if (priv->timeout_millis == t_new)
		return;

	priv->timeout_millis = t_new;

	if (priv->poll_handle) {
		g_source_remove (priv->poll_handle);
		priv->poll_handle = g_timeout_add (priv->timeout_millis, poll_func, this);
	}
}

static void
init_hal_context (StorageTileModel *this)
{
	StorageTileModelPrivate *priv = PRIVATE (this);

	DBusConnection *conn;
	DBusError       error;


	priv->hal_context = libhal_ctx_new ();

	if (! priv->hal_context)
		return;

	dbus_error_init (& error);
	conn = dbus_bus_get (DBUS_BUS_SYSTEM, & error);
	dbus_connection_set_exit_on_disconnect (conn, FALSE);

	if (dbus_error_is_set (& error)) {
		g_warning ("%s: error (%s): [%s]\n", G_STRFUNC, error.name, error.message);

		priv->hal_context = NULL;
		conn              = NULL;
	}

	dbus_error_free (& error);

	if (priv->hal_context && conn) {
		dbus_connection_setup_with_g_main (conn, g_main_context_default ());
		libhal_ctx_set_dbus_connection (priv->hal_context, conn);

		dbus_error_init (& error);
		libhal_ctx_init (priv->hal_context, & error);

		if (dbus_error_is_set (& error)) {
			g_warning ("%s: error (%s): [%s]\n", G_STRFUNC, error.name, error.message);

			priv->hal_context = NULL;
		}

		dbus_error_free (& error);
	}

	tile_attribute_set_pointer (priv->info_attr, priv->info);
}

static gboolean
poll_func (gpointer data)
{
	update_model (STORAGE_TILE_MODEL (data));

	return TRUE;
}

static void
timeout_notify_cb (GConfClient *client, guint conn_id,
                   GConfEntry *entry, gpointer data)
{
	update_timeout (STORAGE_TILE_MODEL (data));
}
