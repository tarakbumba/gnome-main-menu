#include "libslab-utils.h"

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <string.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf-value.h>
#include <libgnome/gnome-url.h>

#define GLOBAL_XDG_PATH_ENV_VAR  "XDG_DATA_DIRS"
#define DEFAULT_GLOBAL_XDG_PATH  "/usr/local/share:/usr/share"
#define DEFAULT_USER_XDG_DIR     ".local/share"
#define TOP_CONFIG_DIR           PACKAGE

#define SYSTEM_BOOKMARK_FILENAME "system-items.xbel"
#define APPS_BOOKMARK_FILENAME   "applications.xbel"
#define DOCS_BOOKMARK_FILENAME   "documents.xbel"
#define DIRS_BOOKMARK_FILENAME   "places.xbel"

static gchar                 *get_data_file_path     (const gchar *);
static GList                 *get_uri_list           (const gchar *);
static void                   save_uri_list          (const gchar *, const GList *);
static GnomeVFSMonitorHandle *add_store_file_monitor (const gchar *,
                                                      GnomeVFSMonitorCallback,
                                                      gpointer);

#define ALTERNATE_DOCPATH_KEY "DocPath"

gboolean
libslab_gtk_image_set_by_id (GtkImage *image, const gchar *id)
{
	GdkPixbuf *pixbuf;

	gint size;
	gint width;
	gint height;

	GtkIconTheme *icon_theme;

	gboolean found;

	gchar *tmp;


	if (! id)
		return FALSE;

	g_object_get (G_OBJECT (image), "icon-size", & size, NULL);

	if (size == GTK_ICON_SIZE_INVALID)
		size = GTK_ICON_SIZE_DND;

	gtk_icon_size_lookup (size, & width, & height);

	if (g_path_is_absolute (id)) {
		pixbuf = gdk_pixbuf_new_from_file_at_size (id, width, height, NULL);

		found = (pixbuf != NULL);

		if (found) {
			gtk_image_set_from_pixbuf (image, pixbuf);

			g_object_unref (pixbuf);
		}
		else
			gtk_image_set_from_stock (image, "gtk-missing-image", size);
	}
	else {
		tmp = g_strdup (id);

		if ( /* file extensions are not copesetic with loading by "name" */
			g_str_has_suffix (tmp, ".png") ||
			g_str_has_suffix (tmp, ".svg") ||
			g_str_has_suffix (tmp, ".xpm")
		)

			tmp [strlen (tmp) - 4] = '\0';

		if (gtk_widget_has_screen (GTK_WIDGET (image)))
			icon_theme = gtk_icon_theme_get_for_screen (
				gtk_widget_get_screen (GTK_WIDGET (image)));
		else
			icon_theme = gtk_icon_theme_get_default ();

		found = gtk_icon_theme_has_icon (icon_theme, tmp);

		if (found)
			gtk_image_set_from_icon_name (image, tmp, size);
		else
			gtk_image_set_from_stock (image, "gtk-missing-image", size);

		g_free (tmp);
	}

	return found;
}

GnomeDesktopItem *
libslab_gnome_desktop_item_new_from_unknown_id (const gchar *id)
{
	GnomeDesktopItem *item;

	GError *error = NULL;


	if (! id)
		return NULL;

	item = gnome_desktop_item_new_from_uri (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_file (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	item = gnome_desktop_item_new_from_basename (id, 0, & error);

	if (! error)
		return item;
	else {
		g_error_free (error);
		error = NULL;
	}

	return NULL;
}

gboolean
libslab_gnome_desktop_item_launch_default (GnomeDesktopItem *item)
{
	GError *error = NULL;

	if (! item)
		return FALSE;

	gnome_desktop_item_launch (item, NULL, GNOME_DESKTOP_ITEM_LAUNCH_ONLY_ONE, & error);

	if (error) {
		g_warning ("error launching %s [%s]\n",
			gnome_desktop_item_get_location (item), error->message);

		g_error_free (error);

		return FALSE;
	}

	return TRUE;
}

gchar *
libslab_gnome_desktop_item_get_docpath (GnomeDesktopItem *item)
{
	gchar *path;

	path = g_strdup (gnome_desktop_item_get_localestring (item, GNOME_DESKTOP_ITEM_DOC_PATH));

	if (! path)
		path = g_strdup (gnome_desktop_item_get_localestring (item, ALTERNATE_DOCPATH_KEY));

	return path;
}

gboolean
libslab_gnome_desktop_item_open_help (GnomeDesktopItem *item)
{
	gchar *doc_path;
	gchar *help_uri;

	GError *error = NULL;

	gboolean retval = FALSE;


	if (! item)
		return retval;

	doc_path = libslab_gnome_desktop_item_get_docpath (item);

	if (doc_path) {
		help_uri = g_strdup_printf ("ghelp:%s", doc_path);

		gnome_url_show (help_uri, & error);

		if (error) {
			g_warning ("error opening %s [%s]\n", help_uri, error->message);

			g_error_free (error);

			retval = FALSE;
		}
		else
			retval = TRUE;

		g_free (help_uri);
		g_free (doc_path);
	}

	return retval;
}

guint32
libslab_get_current_time_millis ()
{
	GTimeVal time_val;
	guint32  time;


	time = gtk_get_current_event_time ();

	if (! time) {
		g_get_current_time (& time_val);

		time = (guint32) (1000L * time_val.tv_sec + time_val.tv_usec / 1000L);
	}

	return time;
}

void
libslab_cclosure_marshal_VOID__POINTER_POINTER (GClosure     *closure,
                                                GValue       *retval,
                                                guint         n_param,
                                                const GValue *param,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data)
{
	libslab_marshal_func_VOID__POINTER_POINTER callback;
	GCClosure *cc = (GCClosure *) closure;
	gpointer data_0, data_1;


	g_return_if_fail (n_param == 3);

	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data_0 = closure->data;
		data_1 = g_value_peek_pointer (param);
	}
	else {
		data_0 = g_value_peek_pointer (param);
		data_1 = closure->data;
	}

	if (marshal_data)
		callback = (libslab_marshal_func_VOID__POINTER_POINTER) marshal_data;
	else
		callback = (libslab_marshal_func_VOID__POINTER_POINTER) cc->callback;

	callback (
		data_0, g_value_peek_pointer (param + 1),
		g_value_peek_pointer (param + 2), data_1);
}

gint
libslab_strcmp (const gchar *a, const gchar *b)
{
	if (! a && ! b)
		return 0;

	if (! a)
		return strcmp ("", b);

	if (! b)
		return strcmp (a, "");

	return strcmp (a, b);
}

gpointer
libslab_get_gconf_value (const gchar *key)
{
	GConfClient *client;
	GConfValue  *value;
	GError      *error = NULL;

	gpointer retval = NULL;

	GList  *list;
	GSList *slist;

	GConfValue *value_i;
	GSList     *node;


	client = gconf_client_get_default ();
	value  = gconf_client_get (client, key, & error);

	if (error || ! value)
		libslab_handle_g_error (& error, "%s: error getting %s", G_GNUC_FUNCTION, key);
	else {
		switch (value->type) {
			case GCONF_VALUE_STRING:
				retval = (gpointer) g_strdup (gconf_value_get_string (value));
				break;

			case GCONF_VALUE_INT:
				retval = GINT_TO_POINTER (gconf_value_get_int (value));
				break;

			case GCONF_VALUE_BOOL:
				retval = GINT_TO_POINTER (gconf_value_get_bool (value));
				break;

			case GCONF_VALUE_LIST:
				list = NULL;
				slist = gconf_value_get_list (value);

				for (node = slist; node; node = node->next) {
					value_i = (GConfValue *) node->data;

					if (value_i->type == GCONF_VALUE_STRING)
						list = g_list_append (
							list, g_strdup (
								gconf_value_get_string (value_i)));
					else if (value_i->type == GCONF_VALUE_INT)
						list = g_list_append (
							list, GINT_TO_POINTER (
								gconf_value_get_int (value_i)));
					else
						;
				}

				retval = (gpointer) list;

				break;

			default:
				break;
		}
	}

	g_object_unref (client);
	gconf_value_free (value);

	return retval;
}

void
libslab_set_gconf_value (const gchar *key, gconstpointer data)
{
	GConfClient *client;
	GConfValue  *value;

	GConfValueType type;
	GConfValueType list_type;

	GSList *slist = NULL;

	GError *error = NULL;

	GConfValue *value_i;
	GList      *node;


	client = gconf_client_get_default ();
	value  = gconf_client_get (client, key, & error);

	if (error) {
		libslab_handle_g_error (&error, "%s: error getting %s", G_GNUC_FUNCTION, key);

		goto exit;
	}

	type = value->type;
	list_type = ((type == GCONF_VALUE_LIST) ?
		gconf_value_get_list_type (value) : GCONF_VALUE_INVALID);

	gconf_value_free (value);
	value = gconf_value_new (type);

	if (type == GCONF_VALUE_LIST)
		gconf_value_set_list_type (value, list_type);

	switch (type) {
		case GCONF_VALUE_STRING:
			gconf_value_set_string (value, g_strdup ((gchar *) data));
			break;

		case GCONF_VALUE_INT:
			gconf_value_set_int (value, GPOINTER_TO_INT (data));
			break;

		case GCONF_VALUE_BOOL:
			gconf_value_set_bool (value, GPOINTER_TO_INT (data));
			break;

		case GCONF_VALUE_LIST:
			for (node = (GList *) data; node; node = node->next) {
				value_i = gconf_value_new (list_type);

				if (list_type == GCONF_VALUE_STRING)
					gconf_value_set_string (value_i, (const gchar *) node->data);
				else if (list_type == GCONF_VALUE_INT)
					gconf_value_set_int (value_i, GPOINTER_TO_INT (node->data));
				else
					g_assert_not_reached ();

				slist = g_slist_append (slist, value_i);
			}

			gconf_value_set_list_nocopy (value, slist);

			break;

		default:
			break;
	}

	gconf_client_set (client, key, value, & error);

	if (error)
		libslab_handle_g_error (&error, "%s: error setting %s", G_GNUC_FUNCTION, key);

exit:

	gconf_value_free (value);
	g_object_unref (client);
}

void
libslab_handle_g_error (GError **error, const gchar *msg_format, ...)
{
	gchar   *msg;
	va_list  args;


	va_start (args, msg_format);
	msg = g_strdup_vprintf (msg_format, args);
	va_end (args);

	if (*error) {
		g_log (
			G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"\nGError raised: [%s]\nuser_message: [%s]\n", (*error)->message, msg);

		g_error_free (*error);

		*error = NULL;
	}
	else
		g_log (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "\nerror raised: [%s]\n", msg);

	g_free (msg);
}

GList *
libslab_get_system_item_uris ()
{
	return get_uri_list (SYSTEM_BOOKMARK_FILENAME);
}

GList *
libslab_get_app_uris ()
{
	return get_uri_list (APPS_BOOKMARK_FILENAME);
}

void
libslab_save_system_item_uris (const GList *uris)
{
	save_uri_list (SYSTEM_BOOKMARK_FILENAME, uris);
}

void
libslab_save_app_uris (const GList *uris)
{
	save_uri_list (APPS_BOOKMARK_FILENAME, uris);
}

GnomeVFSMonitorHandle *
libslab_add_system_item_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	return add_store_file_monitor (SYSTEM_BOOKMARK_FILENAME, callback, user_data);
}

GnomeVFSMonitorHandle *
libslab_add_apps_monitor (GnomeVFSMonitorCallback callback, gpointer user_data)
{
	return add_store_file_monitor (APPS_BOOKMARK_FILENAME, callback, user_data);
}

static GList *
get_uri_list (const gchar *filename)
{
	GBookmarkFile *bm_file;
	gchar         *path;

	gchar **uris_array;
	GList  *uris_list = NULL;

	GError *error = NULL;

	gint i;


	path = get_data_file_path (filename);

	if (! path)
		return NULL;

	bm_file = g_bookmark_file_new ();
	g_bookmark_file_load_from_file (bm_file, path, & error);

	if (! error) {
		uris_array = g_bookmark_file_get_uris (bm_file, NULL);

		for (i = 0; uris_array [i]; ++i)
			uris_list = g_list_append (uris_list, g_strdup (uris_array [i]));
	}
	else
		libslab_handle_g_error (
			& error,
			"%s: couldn't load bookmark file [%s]",
			G_GNUC_FUNCTION, path);

	g_free (path);
	g_strfreev (uris_array);
	g_bookmark_file_free (bm_file);

	return uris_list;
}

static void
save_uri_list (const gchar *filename, const GList *uris)
{
	GBookmarkFile *bm_file;
	gchar         *path;

	gchar *uri;

	GnomeDesktopItem *ditem;

	const GList *node;

	GError *error = NULL;


	path = get_data_file_path (filename);

	if (! path)
		return;

	bm_file = g_bookmark_file_new ();

	for (node = uris; node; node = node->next) {
		uri = (gchar *) node->data;

		ditem = libslab_gnome_desktop_item_new_from_unknown_id (uri);

		if (ditem) {
			g_bookmark_file_set_mime_type (bm_file, uri, "application/x-desktop");
			g_bookmark_file_add_application (
				bm_file, uri,
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_NAME),
				gnome_desktop_item_get_localestring (ditem, GNOME_DESKTOP_ITEM_EXEC));

			gnome_desktop_item_unref (ditem);
		}
	}

	g_bookmark_file_to_file (bm_file, path, & error);

	if (error)
		libslab_handle_g_error (
			& error, "%s: cannot save system item list [%s]",
			G_GNUC_FUNCTION, path);

	g_bookmark_file_free (bm_file);
}

static gchar *
get_data_file_path (const gchar *filename)
{
	GList *dirs = NULL;
	gchar *path = NULL;

	gchar **global_dirs;
	gchar  *path_i;

	GList *node;
	gint   i;


	path_i = g_build_filename (
		g_getenv ("XDG_DATA_HOME"), TOP_CONFIG_DIR, NULL);
	dirs = g_list_append (dirs, path_i);

	path_i = g_build_filename (
		g_get_home_dir (), DEFAULT_USER_XDG_DIR,
		TOP_CONFIG_DIR, NULL);
	dirs = g_list_append (dirs, path_i);

	global_dirs = g_strsplit (g_getenv ("XDG_DATA_DIRS"), ":", 0);

	if (! global_dirs [0]) {
		g_strfreev (global_dirs);
		global_dirs = g_strsplit (DEFAULT_GLOBAL_XDG_PATH, ":", 0);
	}

	for (i = 0; global_dirs [i]; ++i) {
		path_i = g_build_filename (
			global_dirs [i], TOP_CONFIG_DIR, NULL);
		dirs = g_list_append (dirs, path_i);
	}

	g_strfreev (global_dirs);

	for (node = dirs; ! path && node; node = node->next) {
		path = g_build_filename (
			(gchar *) node->data, filename, NULL);

		if (! g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_free (path);
			path = NULL;
		}
	}

	for (node = dirs; node; node = node->next)
		g_free (node->data);
	g_list_free (dirs);

	return path;
}

static GnomeVFSMonitorHandle *
add_store_file_monitor (const gchar *filename, GnomeVFSMonitorCallback callback, gpointer user_data)
{
	GnomeVFSMonitorHandle *handle;
	gchar                 *path;
	gchar                 *uri;


	path = get_data_file_path (filename);
	uri  = g_filename_to_uri (path, NULL, NULL);

	gnome_vfs_monitor_add (& handle, uri, GNOME_VFS_MONITOR_FILE, callback, user_data);

	g_free (path);
	g_free (uri);

	return handle;
}
