/*
 * This file is part of libtile.
 *
 * Copyright (c) 2006 Novell, Inc.
 *
 * Libtile is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * Libtile is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libslab; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "directory-tile.h"

#include <glib/gi18n.h>
#include <string.h>
#include <libgnomeui/gnome-icon-lookup.h>
#include <libgnomevfs/gnome-vfs.h>

#include "slab-gnome-util.h"
#include "gnome-utils.h"

#define GCONF_SEND_TO_CMD_KEY       "/desktop/gnome/applications/main-menu/file-area/file_send_to_cmd"
#define GCONF_ENABLE_DELETE_KEY_DIR "/apps/nautilus/preferences"
#define GCONF_ENABLE_DELETE_KEY     GCONF_ENABLE_DELETE_KEY_DIR "/enable_delete"

G_DEFINE_TYPE (DirectoryTile, directory_tile, NAMEPLATE_TILE_TYPE)

static void directory_tile_finalize (GObject *);
static void directory_tile_style_set (GtkWidget *, GtkStyle *);

static void directory_tile_private_setup (DirectoryTile *);
static void load_image (DirectoryTile *);

static GtkWidget *create_header (const gchar *);

static void header_size_allocate_cb (GtkWidget *, GtkAllocation *, gpointer);

static void open_trigger (Tile *, TileEvent *, TileAction *);
static void rename_trigger (Tile *, TileEvent *, TileAction *);
static void move_to_trash_trigger (Tile *, TileEvent *, TileAction *);
static void delete_trigger (Tile *, TileEvent *, TileAction *);
static void send_to_trigger (Tile *, TileEvent *, TileAction *);

static void rename_entry_activate_cb (GtkEntry *, gpointer);
static gboolean rename_entry_key_release_cb (GtkWidget *, GdkEventKey *, gpointer);

static void gconf_enable_delete_cb (GConfClient *, guint, GConfEntry *, gpointer);

typedef struct
{
	gchar *basename;
	
	GtkBin *header_bin;
	
	gboolean renaming;
	gboolean image_is_broken;
	
	gboolean delete_enabled;
	guint gconf_conn_id;
} DirectoryTilePrivate;

#define DIRECTORY_TILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), DIRECTORY_TILE_TYPE, DirectoryTilePrivate))

static void directory_tile_class_init (DirectoryTileClass *this_class)
{
	GObjectClass *g_obj_class = G_OBJECT_CLASS (this_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (this_class);

	g_obj_class->finalize = directory_tile_finalize;

	widget_class->style_set = directory_tile_style_set;

	g_type_class_add_private (this_class, sizeof (DirectoryTilePrivate));
}

GtkWidget *
directory_tile_new (const gchar *in_uri)
{
	DirectoryTile *this;
	DirectoryTilePrivate *priv;

	gchar *uri;
	GtkWidget *image;
	GtkWidget *header;
	GtkMenu *context_menu;

	GtkContainer *menu_ctnr;
	GtkWidget *menu_item;

	TileAction *action;

	gchar *basename;

	gchar *markup;

	AtkObject *accessible;
	
	gchar *filename;
	gchar *tooltip_text;

  
	uri = g_strdup (in_uri);

	image = gtk_image_new ();

	markup = g_path_get_basename (uri);
	basename = gnome_vfs_unescape_string (markup, NULL);
	g_free (markup);

	header = create_header (basename);

	filename = g_filename_from_uri (uri, NULL, NULL);

  	if (filename)
		tooltip_text = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
	else
		tooltip_text = NULL;

	g_free (filename);
	
	context_menu = GTK_MENU (gtk_menu_new ());

	this = g_object_new (
		DIRECTORY_TILE_TYPE,
		"tile-uri",          uri,
		"nameplate-image",   image,
		"nameplate-header",  header,
		"nameplate-tooltip", tooltip_text,
		"context-menu",      context_menu,
		NULL);

	g_free (uri);

	priv = DIRECTORY_TILE_GET_PRIVATE (this);
	priv->basename    = g_strdup (basename);
	priv->header_bin  = GTK_BIN (header);

	directory_tile_private_setup (this);

	TILE (this)->actions = g_new0 (TileAction *, 6);
	TILE (this)->n_actions = 6;

	menu_ctnr = GTK_CONTAINER (TILE (this)->context_menu);

	/* make open with default action */

	markup = g_markup_printf_escaped (_("<b>Open</b>"));
	action = tile_action_new (TILE (this), open_trigger, markup, TILE_ACTION_OPENS_NEW_WINDOW);
	g_free (markup);

	TILE (this)->default_action = action;

	menu_item = GTK_WIDGET (GTK_WIDGET (tile_action_get_menu_item (action)));

	TILE (this)->actions [DIRECTORY_TILE_ACTION_OPEN] = action;

	gtk_container_add (menu_ctnr, menu_item);

	/* insert separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_container_add (menu_ctnr, menu_item);

	/* make rename action */

	action = tile_action_new (TILE (this), rename_trigger, _("Rename..."), 0);
	TILE (this)->actions[DIRECTORY_TILE_ACTION_RENAME] = action;

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	gtk_container_add (menu_ctnr, menu_item);

	/* insert separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_container_add (menu_ctnr, menu_item);

	/* make move to trash action */

	action = tile_action_new (TILE (this), move_to_trash_trigger, _("Move to Trash"), 0);
	TILE (this)->actions[DIRECTORY_TILE_ACTION_MOVE_TO_TRASH] = action;

	menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	gtk_container_add (menu_ctnr, menu_item);

	/* make delete action */

	if (priv->delete_enabled)
	{
		action = tile_action_new (TILE (this), delete_trigger, _("Delete"), 0);
		TILE (this)->actions[DIRECTORY_TILE_ACTION_DELETE] = action;

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_container_add (menu_ctnr, menu_item);
	}

	/* insert separator */

	menu_item = gtk_separator_menu_item_new ();
	gtk_container_add (menu_ctnr, menu_item);

	/* make send to action */

	/* Only allow Send To for local files, ideally this would use something
	 * equivalent to gnome_vfs_uri_is_local, but that method will stat the file and
	 * that can hang in some conditions. */

	if (!strncmp (TILE (this)->uri, "file://", 7))
	{
		action = tile_action_new (TILE (this), send_to_trigger, _("Send To..."),
			TILE_ACTION_OPENS_NEW_WINDOW);

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
	}
	else
	{
		action = NULL;

		menu_item = gtk_menu_item_new_with_label (_("Send To..."));
		gtk_widget_set_sensitive (menu_item, FALSE);
	}

	TILE (this)->actions[DIRECTORY_TILE_ACTION_SEND_TO] = action;

	gtk_container_add (menu_ctnr, menu_item);

	gtk_widget_show_all (GTK_WIDGET (TILE (this)->context_menu));

	load_image (this);

	accessible = gtk_widget_get_accessible (GTK_WIDGET (this));
	if (basename)
	  atk_object_set_name (accessible, basename);

	g_free (basename);

	return GTK_WIDGET (this);
}

static void
directory_tile_private_setup (DirectoryTile *tile)
{
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (tile);

	GConfClient *client;


	priv->renaming = FALSE;

	priv->delete_enabled =
		(gboolean) GPOINTER_TO_INT (get_gconf_value (GCONF_ENABLE_DELETE_KEY));

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, GCONF_ENABLE_DELETE_KEY_DIR, GCONF_CLIENT_PRELOAD_NONE, NULL);
	priv->gconf_conn_id =
		connect_gconf_notify (GCONF_ENABLE_DELETE_KEY, gconf_enable_delete_cb, tile);

	g_object_unref (client);
}

static void
directory_tile_init (DirectoryTile *tile)
{
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (tile);

	priv->basename = NULL;
	priv->header_bin = NULL;
	priv->renaming = FALSE;
	priv->image_is_broken = TRUE;
	priv->delete_enabled = FALSE;
	priv->gconf_conn_id = 0;
}

static void
directory_tile_finalize (GObject *g_object)
{
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (g_object);

	GConfClient *client;

	g_free (priv->basename);

	client = gconf_client_get_default ();

	gconf_client_notify_remove (client, priv->gconf_conn_id);
	gconf_client_remove_dir (client, GCONF_ENABLE_DELETE_KEY_DIR, NULL);

	g_object_unref (client);

	(* G_OBJECT_CLASS (directory_tile_parent_class)->finalize) (g_object);
}

static void
directory_tile_style_set (GtkWidget *widget, GtkStyle *prev_style)
{
	load_image (DIRECTORY_TILE (widget));
}

static void
load_image (DirectoryTile *tile)
{
	DIRECTORY_TILE_GET_PRIVATE (tile)->image_is_broken = slab_load_image (
		GTK_IMAGE (NAMEPLATE_TILE (tile)->image), GTK_ICON_SIZE_DND, "gnome-fs-directory");
}

static GtkWidget *
create_header (const gchar *name)
{
	GtkWidget *header_bin;
	GtkWidget *header;

	header = gtk_label_new (name);
	gtk_label_set_ellipsize (GTK_LABEL (header), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	header_bin = gtk_alignment_new (0.0, 0.5, 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (header_bin), header);

	g_signal_connect (G_OBJECT (header), "size-allocate", G_CALLBACK (header_size_allocate_cb),
		NULL);

	return header_bin;
}

static void
header_size_allocate_cb (GtkWidget *widget, GtkAllocation *alloc, gpointer user_data)
{
	gtk_widget_set_size_request (widget, alloc->width, -1);
}

static void
rename_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
	DirectoryTile *tile = DIRECTORY_TILE (user_data);
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (tile);

	GnomeVFSURI *src_uri;
	GnomeVFSURI *dst_uri;

	gchar *dirname;
	gchar *dst_path;
	gchar *dst_uri_str;

	GtkWidget *child;
	GtkWidget *header;

	GnomeVFSResult retval;


	if (strlen (gtk_entry_get_text (entry)) < 1)
		return;

	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	dirname = gnome_vfs_uri_extract_dirname (src_uri);

	dst_path = g_build_filename (dirname, gtk_entry_get_text (entry), NULL);

	dst_uri = gnome_vfs_uri_new (dst_path);

	retval = gnome_vfs_xfer_uri (src_uri, dst_uri, GNOME_VFS_XFER_REMOVESOURCE,
		GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_OVERWRITE_MODE_SKIP, NULL, NULL);

	dst_uri_str = gnome_vfs_uri_to_string (dst_uri, GNOME_VFS_URI_HIDE_NONE);

	if (retval == GNOME_VFS_OK) {
		g_free (priv->basename);
		priv->basename = g_strdup (gtk_entry_get_text (entry));
	}
	else
		g_warning ("unable to move [%s] to [%s]\n", TILE (tile)->uri, dst_uri_str);

	header = gtk_label_new (priv->basename);
	gtk_misc_set_alignment (GTK_MISC (header), 0.0, 0.5);

	child = gtk_bin_get_child (priv->header_bin);

	if (child)
		gtk_widget_destroy (child);

	gtk_container_add (GTK_CONTAINER (priv->header_bin), header);

	gtk_widget_show (header);

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (dst_uri);

	g_free (dirname);
	g_free (dst_path);
	g_free (dst_uri_str);
}

static gboolean
rename_entry_key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	return TRUE;
}

static void
gconf_enable_delete_cb (GConfClient *client, guint conn_id, GConfEntry *entry, gpointer user_data)
{
	Tile *tile = TILE (user_data);
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (user_data);

	GtkMenuShell *menu;
	gboolean delete_enabled;

	TileAction *action;
	GtkWidget *menu_item;

	menu = GTK_MENU_SHELL (tile->context_menu);

	delete_enabled = gconf_value_get_bool (entry->value);

	if (delete_enabled == priv->delete_enabled)
		return;

	priv->delete_enabled = delete_enabled;

	if (priv->delete_enabled)
	{
		action = tile_action_new (tile, delete_trigger, _("Delete"), 0);
		tile->actions[DIRECTORY_TILE_ACTION_DELETE] = action;

		menu_item = GTK_WIDGET (tile_action_get_menu_item (action));
		gtk_menu_shell_insert (menu, menu_item, 7);

		gtk_widget_show_all (menu_item);
	}
	else
	{
		g_object_unref (tile->actions[DIRECTORY_TILE_ACTION_DELETE]);

		tile->actions[DIRECTORY_TILE_ACTION_DELETE] = NULL;
	}
}

static void
open_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	gchar *filename;
	gchar *dirname;
	gchar *uri;

	gchar *cmd;

	filename = g_filename_from_uri (TILE (tile)->uri, NULL, NULL);
	dirname = g_path_get_dirname (filename);
	uri = g_filename_to_uri (dirname, NULL, NULL);

	if (!uri)
		g_warning ("error getting dirname for [%s]\n", TILE (tile)->uri);
	else
	{
		cmd = string_replace_once (get_slab_gconf_string (SLAB_FILE_MANAGER_OPEN_CMD),
			"FILE_URI", uri);

		spawn_process (cmd);

		g_free (cmd);
	}

	g_free (filename);
	g_free (dirname);
	g_free (uri);
}

static void
rename_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	DirectoryTilePrivate *priv = DIRECTORY_TILE_GET_PRIVATE (tile);

	GtkWidget *child;
	GtkWidget *entry;


	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), priv->basename);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

	child = gtk_bin_get_child (priv->header_bin);

	if (child)
		gtk_widget_destroy (child);

	gtk_container_add (GTK_CONTAINER (priv->header_bin), entry);

	g_signal_connect (G_OBJECT (entry), "activate", G_CALLBACK (rename_entry_activate_cb), tile);

	g_signal_connect (G_OBJECT (entry), "key_release_event",
		G_CALLBACK (rename_entry_key_release_cb), NULL);

	gtk_widget_show (entry);
	gtk_widget_grab_focus (entry);
}

static void
move_to_trash_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	GnomeVFSURI *src_uri;
	GnomeVFSURI *trash_uri;

	gchar *file_name;
	gchar *trash_uri_str;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	gnome_vfs_find_directory (src_uri, GNOME_VFS_DIRECTORY_KIND_TRASH, &trash_uri,
		FALSE, FALSE, 0777);

	if (!trash_uri) {
		g_warning ("unable to find trash location\n");

		return;
	}

	file_name = gnome_vfs_uri_extract_short_name (src_uri);

	if (!file_name) {
		g_warning ("unable to extract short name from [%s]\n",
			gnome_vfs_uri_to_string (src_uri, GNOME_VFS_URI_HIDE_NONE));

		return;
	}

	trash_uri = gnome_vfs_uri_append_file_name (trash_uri, file_name);

	retval = gnome_vfs_xfer_uri (src_uri, trash_uri, GNOME_VFS_XFER_REMOVESOURCE,
		GNOME_VFS_XFER_ERROR_MODE_ABORT, GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, NULL, NULL);

	if (retval != GNOME_VFS_OK) {
		trash_uri_str = gnome_vfs_uri_to_string (trash_uri, GNOME_VFS_URI_HIDE_NONE);

		g_warning ("unable to move [%s] to the trash [%s]\n", TILE (tile)->uri,
			trash_uri_str);

		g_free (trash_uri_str);
	}

	gnome_vfs_uri_unref (src_uri);
	gnome_vfs_uri_unref (trash_uri);

	g_free (file_name);
}

static void
delete_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	GnomeVFSURI *src_uri;
	GList *list = NULL;

	GnomeVFSResult retval;


	src_uri = gnome_vfs_uri_new (TILE (tile)->uri);

	list = g_list_append (list, src_uri);

	retval = gnome_vfs_xfer_delete_list (list, GNOME_VFS_XFER_ERROR_MODE_ABORT,
		GNOME_VFS_XFER_REMOVESOURCE, NULL, NULL);

	if (retval != GNOME_VFS_OK)
		g_warning ("unable to delete [%s]\n", TILE (tile)->uri);

	gnome_vfs_uri_unref (src_uri);
	g_list_free (list);
}

static void
send_to_trigger (Tile *tile, TileEvent *event, TileAction *action)
{
	gchar *cmd;
	gchar **argv;

	gchar *filename;
	gchar *dirname;
	gchar *basename;

	GError *error = NULL;

	gchar *tmp;
	gint i;

	cmd = (gchar *) get_gconf_value (GCONF_SEND_TO_CMD_KEY);
	argv = g_strsplit (cmd, " ", 0);

	filename = g_filename_from_uri (TILE (tile)->uri, NULL, NULL);
	dirname = g_path_get_dirname (filename);
	basename = g_path_get_basename (filename);

	for (i = 0; argv[i]; ++i)
	{
		if (strstr (argv[i], "DIRNAME"))
		{
			tmp = string_replace_once (argv[i], "DIRNAME", dirname);
			g_free (argv[i]);
			argv[i] = tmp;
		}

		if (strstr (argv[i], "BASENAME"))
		{
			tmp = string_replace_once (argv[i], "BASENAME", basename);
			g_free (argv[i]);
			argv[i] = tmp;
		}
	}

	gdk_spawn_on_screen (gtk_widget_get_screen (GTK_WIDGET (tile)), NULL, argv, NULL,
		G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error)
		handle_g_error (&error, "error in %s", __FUNCTION__);

	g_free (cmd);
	g_free (filename);
	g_free (dirname);
	g_free (basename);
	g_strfreev (argv);
}