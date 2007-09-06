#include "desktop-item-tile-model.h"

#include <string.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libgnomevfs/gnome-vfs.h>

#include "libslab-utils.h"
#include "bookmark-agent.h"

typedef struct {
	gchar                   *uri;
	GnomeDesktopItem        *ditem;

	BookmarkStoreType        store_type;
	BookmarkAgent           *user_agent;
	BookmarkAgent           *recent_agent;

	TileAttribute           *icon_id_attr;
	TileAttribute           *name_attr;
	TileAttribute           *desc_attr;
	TileAttribute           *comment_attr;
	TileAttribute           *help_available_attr;
	TileAttribute           *is_in_store_attr;
	TileAttribute           *store_status_attr;
	TileAttribute           *autostart_status_attr;
	TileAttribute           *can_manage_package_attr;
} DesktopItemTileModelPrivate;

#define PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DESKTOP_ITEM_TILE_MODEL_TYPE, DesktopItemTileModelPrivate))

enum {
	PROP_0,
	PROP_USER_STORE_TYPE
};

#define UPGRADE_PACKAGE_KEY   "/desktop/gnome/applications/main-menu/upgrade_package_command"
#define UNINSTALL_PACKAGE_KEY "/desktop/gnome/applications/main-menu/uninstall_package_command"

static void this_class_init (DesktopItemTileModelClass *);
static void this_init       (DesktopItemTileModel *);

static void get_property (GObject *, guint, GValue *, GParamSpec *);
static void set_property (GObject *, guint, const GValue *, GParamSpec *);
static void finalize     (GObject *);

static void update_model (DesktopItemTileModel *);
static DesktopItemAutostartStatus get_autostart_status (DesktopItemTileModel *);
static void add_to_autostart_dir (DesktopItemTileModel *);
static void remove_from_autostart_dir (DesktopItemTileModel *);
static gboolean verify_package_management_commands (void);
static void run_package_management_command (DesktopItemTileModel *, const gchar *);

static void uri_attr_notify_cb          (GObject *, GParamSpec *, gpointer);
static void user_store_items_notify_cb  (GObject *, GParamSpec *, gpointer);
static void user_store_status_notify_cb (GObject *, GParamSpec *, gpointer);

static GObjectClass *this_parent_class = NULL;

GType
desktop_item_tile_model_get_type ()
{
	static GType type_id = 0;

	if (G_UNLIKELY (type_id == 0))
		type_id = g_type_register_static_simple (
			TILE_MODEL_TYPE, "DesktopItemTileModel",
			sizeof (DesktopItemTileModelClass), (GClassInitFunc) this_class_init,
			sizeof (DesktopItemTileModel), (GInstanceInitFunc) this_init, 0);

	return type_id;
}

DesktopItemTileModel *
desktop_item_tile_model_new (const gchar *desktop_item_id)
{
	DesktopItemTileModel        *this;
	DesktopItemTileModelPrivate *priv;

	GnomeDesktopItem *ditem;


	ditem = libslab_gnome_desktop_item_new_from_unknown_id (desktop_item_id);

	if (! ditem)
		return NULL;

	this = g_object_new (DESKTOP_ITEM_TILE_MODEL_TYPE, NULL);
	priv = PRIVATE (this);

	priv->uri = g_strdup (gnome_desktop_item_get_location (ditem));
	tile_attribute_set_string (tile_model_get_uri_attr (TILE_MODEL (this)), priv->uri);

	priv->ditem = ditem;

	priv->user_agent   = bookmark_agent_get_instance (priv->store_type);
	priv->recent_agent = bookmark_agent_get_instance (BOOKMARK_STORE_RECENT_APPS);

	priv->icon_id_attr            = tile_attribute_new (G_TYPE_STRING);
	priv->name_attr               = tile_attribute_new (G_TYPE_STRING);
	priv->desc_attr               = tile_attribute_new (G_TYPE_STRING);
	priv->comment_attr            = tile_attribute_new (G_TYPE_STRING);
	priv->help_available_attr     = tile_attribute_new (G_TYPE_BOOLEAN);
	priv->is_in_store_attr        = tile_attribute_new (G_TYPE_BOOLEAN);
	priv->store_status_attr       = tile_attribute_new (G_TYPE_INT);
	priv->autostart_status_attr   = tile_attribute_new (G_TYPE_INT);
	priv->can_manage_package_attr = tile_attribute_new (G_TYPE_BOOLEAN);

	update_model (this);

	tile_attribute_set_int (
		priv->store_status_attr, bookmark_agent_get_status (priv->user_agent));

	g_signal_connect (
		tile_model_get_uri_attr (TILE_MODEL (this)), "notify::" TILE_ATTRIBUTE_VALUE_PROP,
		G_CALLBACK (uri_attr_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->user_agent), "notify::" BOOKMARK_AGENT_ITEMS_PROP,
		G_CALLBACK (user_store_items_notify_cb), this);

	g_signal_connect (
		G_OBJECT (priv->user_agent), "notify::" BOOKMARK_AGENT_STORE_STATUS_PROP,
		G_CALLBACK (user_store_status_notify_cb), this);

	return this;
}

TileAttribute *
desktop_item_tile_model_get_icon_id_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->icon_id_attr;
}

TileAttribute *
desktop_item_tile_model_get_name_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->name_attr;
}

TileAttribute *
desktop_item_tile_model_get_description_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->desc_attr;
}

TileAttribute *
desktop_item_tile_model_get_comment_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->comment_attr;
}

TileAttribute *
desktop_item_tile_model_get_help_available_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->help_available_attr;
}

TileAttribute *
desktop_item_tile_model_get_is_in_store_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->is_in_store_attr;
}

TileAttribute *
desktop_item_tile_model_get_store_status_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->store_status_attr;
}

TileAttribute *
desktop_item_tile_model_get_autostart_status_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->autostart_status_attr;
}

TileAttribute *
desktop_item_tile_model_get_can_manage_package_attr (DesktopItemTileModel *this)
{
	return PRIVATE (this)->can_manage_package_attr;
}

void
desktop_item_tile_model_start (DesktopItemTileModel *this)
{
	libslab_gnome_desktop_item_launch_default (PRIVATE (this)->ditem);
}

void
desktop_item_tile_model_open_help (DesktopItemTileModel *this)
{
	libslab_gnome_desktop_item_open_help (PRIVATE (this)->ditem);
}

void
desktop_item_tile_model_user_store_toggle (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	BookmarkItem *item;


	if (bookmark_agent_has_item (priv->user_agent, priv->uri))
		bookmark_agent_remove_item (priv->user_agent, priv->uri);
	else {
		item = g_new0 (BookmarkItem, 1);
		item->uri       = priv->uri;
		item->mime_type = "application/x-desktop";

		bookmark_agent_add_item (priv->user_agent, item);
		g_free (item);
	}
}

void
desktop_item_tile_model_autostart_toggle (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	DesktopItemAutostartStatus status;


	status = get_autostart_status (this);

	switch (get_autostart_status (this)) {
		case APP_IN_USER_AUTOSTART_DIR:
			remove_from_autostart_dir (this);
			status = APP_NOT_IN_AUTOSTART_DIR;

			break;

		case APP_NOT_IN_AUTOSTART_DIR:
			add_to_autostart_dir (this);
			status = APP_IN_USER_AUTOSTART_DIR;

			break;

		default:
			break;
	}

	tile_attribute_set_int (priv->autostart_status_attr, status);
}

void
desktop_item_tile_model_upgrade_package (DesktopItemTileModel *this)
{
	run_package_management_command (this, UPGRADE_PACKAGE_KEY);
}

void
desktop_item_tile_model_uninstall_package (DesktopItemTileModel *this)
{
	run_package_management_command (this, UNINSTALL_PACKAGE_KEY);
}

static void
this_class_init (DesktopItemTileModelClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);

	GParamSpec *store_type_pspec;


	g_obj_class->finalize     = finalize;
	g_obj_class->get_property = get_property;
	g_obj_class->set_property = set_property;

	store_type_pspec = g_param_spec_int (
		DESKTOP_ITEM_TILE_MODEL_STORE_TYPE_PROP, DESKTOP_ITEM_TILE_MODEL_STORE_TYPE_PROP,
		"The BookmarkStoreType associated with this model",
		BOOKMARK_STORE_USER_APPS, BOOKMARK_STORE_SYSTEM, BOOKMARK_STORE_USER_APPS,
		G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_NAME |
		G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);

	g_object_class_install_property (g_obj_class, PROP_USER_STORE_TYPE, store_type_pspec);

	g_type_class_add_private (this_class, sizeof (DesktopItemTileModelPrivate));

	this_parent_class = g_type_class_peek_parent (this_class);
}

static void
this_init (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	priv->uri                     = NULL;
	priv->ditem                   = NULL;

	priv->store_type              = BOOKMARK_STORE_USER_APPS;
	priv->user_agent              = NULL;
	priv->recent_agent            = NULL;

	priv->icon_id_attr            = NULL;
	priv->name_attr               = NULL;
	priv->desc_attr               = NULL;
	priv->comment_attr            = NULL;
	priv->help_available_attr     = NULL;
	priv->is_in_store_attr        = NULL;
	priv->store_status_attr       = NULL;
	priv->autostart_status_attr   = NULL;
	priv->can_manage_package_attr = NULL;
}

static void
get_property (GObject *g_obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	if (prop_id != PROP_USER_STORE_TYPE)
		return;

	g_value_set_int (value, PRIVATE (g_obj)->store_type);
}

static void
set_property (GObject *g_obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (g_obj);

	BookmarkStoreType store_type;


	if (prop_id != PROP_USER_STORE_TYPE)
		return;

	store_type = g_value_get_int (value);

	if (priv->store_type == store_type)
		return;

	priv->store_type = store_type;

	g_object_unref (priv->user_agent);
	priv->user_agent = bookmark_agent_get_instance (priv->store_type);

	tile_attribute_set_int (
		priv->store_status_attr, bookmark_agent_get_status (priv->user_agent));
	tile_attribute_set_boolean (
		priv->is_in_store_attr,
		bookmark_agent_has_item (priv->user_agent, priv->uri));
}

static void
finalize (GObject *g_obj)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (g_obj);

	g_free (priv->uri);
	gnome_desktop_item_unref (priv->ditem);

	g_object_unref (priv->user_agent);
	g_object_unref (priv->recent_agent);

	g_object_unref (priv->icon_id_attr);
	g_object_unref (priv->name_attr);
	g_object_unref (priv->comment_attr);
	g_object_unref (priv->help_available_attr);
	g_object_unref (priv->is_in_store_attr);
	g_object_unref (priv->store_status_attr);
	g_object_unref (priv->autostart_status_attr);
	g_object_unref (priv->can_manage_package_attr);

	G_OBJECT_CLASS (this_parent_class)->finalize (g_obj);
}

static void
update_model (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	tile_attribute_set_string (
		priv->icon_id_attr,
		gnome_desktop_item_get_localestring (priv->ditem, "Icon"));

	tile_attribute_set_string (
		priv->name_attr,
		gnome_desktop_item_get_localestring (priv->ditem, "Name"));

	tile_attribute_set_string (
		priv->desc_attr,
		gnome_desktop_item_get_localestring (priv->ditem, "GenericName"));

	tile_attribute_set_string (
		priv->comment_attr,
		gnome_desktop_item_get_localestring (priv->ditem, "Comment"));

	tile_attribute_set_boolean (
		priv->help_available_attr,
		(gnome_desktop_item_get_string (priv->ditem, "DocPath") != NULL));

	tile_attribute_set_boolean (
		priv->is_in_store_attr,
		bookmark_agent_has_item (priv->user_agent, priv->uri));

	tile_attribute_set_int (
		priv->store_status_attr,
		bookmark_agent_get_status (priv->user_agent));

	tile_attribute_set_int (
		priv->autostart_status_attr,
		get_autostart_status (this));

	tile_attribute_set_boolean (
		priv->can_manage_package_attr,
		verify_package_management_commands ());
}

static DesktopItemAutostartStatus
get_autostart_status (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	gchar *filename;
	gchar *basename;

	const gchar * const * global_dirs;
	gchar *global_target;
	gchar *user_target;

	DesktopItemAutostartStatus retval;

	gint i;

	
	filename = g_filename_from_uri (priv->uri, NULL, NULL);

	if (! filename)
		return APP_NOT_ELIGIBLE;

	basename = g_path_get_basename (filename);

	retval = APP_NOT_IN_AUTOSTART_DIR;

	global_dirs = g_get_system_config_dirs();

	for (i = 0; global_dirs [i] && retval != APP_IN_SYSTEM_AUTOSTART_DIR; ++i) {
		global_target = g_build_filename (global_dirs [i], "autostart", basename, NULL);

		if (g_file_test (global_target, G_FILE_TEST_EXISTS))
			retval = APP_IN_SYSTEM_AUTOSTART_DIR;

		g_free (global_target);
	}

	if (retval == APP_IN_SYSTEM_AUTOSTART_DIR)
		goto exit;

	/* gnome-session currently checks these dirs also. see startup-programs.c */

	global_dirs = g_get_system_data_dirs();

	for (i = 0; global_dirs [i] && retval != APP_IN_SYSTEM_AUTOSTART_DIR; ++i) {
		global_target = g_build_filename (global_dirs [i], "gnome", "autostart", basename, NULL);

		if (g_file_test (global_target, G_FILE_TEST_EXISTS))
			retval = APP_IN_SYSTEM_AUTOSTART_DIR;

		g_free (global_target);
	}

	if (retval == APP_IN_SYSTEM_AUTOSTART_DIR)
		goto exit;

	user_target = g_build_filename (g_get_user_config_dir (), "autostart", basename, NULL);

	if (g_file_test (user_target, G_FILE_TEST_EXISTS))
		retval = APP_IN_USER_AUTOSTART_DIR;

	g_free (user_target);

exit:

	g_free (basename);
	g_free (filename);

	return retval;
}

static void
add_to_autostart_dir (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	gchar *filename;
	gchar *basename;

	gchar *autostart_dir_path;
	gchar *autostart_path;
	gchar *autostart_uri;

	GnomeVFSURI *src;
	GnomeVFSURI *dst;

	GnomeVFSResult retval;


	filename = g_filename_from_uri (priv->uri, NULL, NULL);

	g_return_if_fail (filename != NULL);

	basename = g_path_get_basename (filename);

	autostart_dir_path = g_build_filename (g_get_user_config_dir (), "autostart", NULL);

	if (! g_file_test (autostart_dir_path, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (autostart_dir_path, 0700);

	autostart_path = g_build_filename (autostart_dir_path, basename, NULL);
	autostart_uri  = g_filename_to_uri (autostart_path, NULL, NULL);

	src = gnome_vfs_uri_new (priv->uri);
	dst = gnome_vfs_uri_new (autostart_uri);

	retval = gnome_vfs_xfer_uri (
		src, dst, GNOME_VFS_XFER_DEFAULT, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_OVERWRITE_MODE_SKIP, NULL, NULL);

	if (retval != GNOME_VFS_OK)
		g_warning ("error copying [%s] to [%s].", priv->uri, autostart_uri);

	g_free (filename);
	g_free (basename);
	g_free (autostart_dir_path);
	g_free (autostart_path);
	g_free (autostart_uri);
	gnome_vfs_uri_unref (src);
	gnome_vfs_uri_unref (dst);
}

static void
remove_from_autostart_dir (DesktopItemTileModel *this)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	gchar *filename;
	gchar *basename;
	gchar *autostart_path;

	gboolean deletable;


	filename = g_filename_from_uri (priv->uri, NULL, NULL);

	g_return_if_fail (filename);

	basename = g_path_get_basename (filename);
	autostart_path = g_build_filename (
		g_get_user_config_dir (), "autostart", basename, NULL);

	deletable =
		  g_file_test (autostart_path, G_FILE_TEST_EXISTS) &&
		! g_file_test (autostart_path, G_FILE_TEST_IS_DIR);

	if (deletable)
		g_unlink (autostart_path);

	g_free (filename);
	g_free (basename);
	g_free (autostart_path);
}

static gboolean
verify_package_management_commands ()
{
	gchar *cmd;
	gchar *path;
	gchar *args;

	gboolean retval;


	cmd = (gchar *) libslab_get_gconf_value (UPGRADE_PACKAGE_KEY);

	if (! cmd)
		return FALSE;

	args = strchr (cmd, ' ');

	if (args)
		*args = '\0';

	path = g_find_program_in_path (cmd);

	retval = (path != NULL);

	g_free (cmd);
	g_free (path);

	return retval;
}

static void
run_package_management_command (DesktopItemTileModel *this, const gchar *gconf_key)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	gchar **argv;
	gint    retval;

	gchar *package_name;
	gchar *cmd_template;
	gchar *cmd;

	GError *error = NULL;


	argv = g_new (gchar *, 6);
	argv [0] = "rpm";
	argv [1] = "-qf";
	argv [2] = "--qf";
	argv [3] = "%{NAME}";
	argv [4] = g_filename_from_uri (priv->uri, NULL, NULL);
	argv [5] = NULL;

	g_spawn_sync (
		NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
		& package_name, NULL, & retval, & error);

	if (error) {
		libslab_handle_g_error (
			& error, "%s: can't find package name [%s]\n", G_STRFUNC, argv [4]);

		retval = -1;
	}

	g_free (argv [4]);
	g_free (argv);

	if (retval)
		goto exit;

	if (! package_name)
		return;

	cmd_template = (gchar *) libslab_get_gconf_value (gconf_key);
	cmd = libslab_string_replace_once (cmd_template, "PACKAGE_NAME", package_name);

	libslab_spawn_command (cmd);

	g_free (cmd_template);
	g_free (cmd);

exit:

	g_free (package_name);
}

static void
uri_attr_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	DesktopItemTileModel        *this = DESKTOP_ITEM_TILE_MODEL (data);
	DesktopItemTileModelPrivate *priv = PRIVATE (this);

	const gchar *uri;


	uri = tile_model_get_uri (TILE_MODEL (this));

	if (libslab_strcmp (uri, priv->uri)) {
		g_free (priv->uri);
		priv->uri = g_strdup (uri);
		update_model (this);
	}
}

static void
user_store_items_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (data);

	tile_attribute_set_boolean (
		priv->is_in_store_attr,
		bookmark_agent_has_item (priv->user_agent, priv->uri));
}

static void
user_store_status_notify_cb (GObject *g_obj, GParamSpec *pspec, gpointer data)
{
	DesktopItemTileModelPrivate *priv = PRIVATE (data);

	tile_attribute_set_int (
		priv->store_status_attr, bookmark_agent_get_status (priv->user_agent));
}
