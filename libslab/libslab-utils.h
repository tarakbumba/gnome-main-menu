#ifndef __GNOME_UTILS_H__
#define __GNOME_UTILS_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomevfs/gnome-vfs.h>

#if GLIB_CHECK_VERSION (2, 12, 0)
#	define USE_G_BOOKMARK

#	define LibSlabBookmarkFile                    GBookmarkFile

#	define libslab_bookmark_file_new              g_bookmark_file_new
#	define libslab_bookmark_file_free             g_bookmark_file_free

#	define libslab_bookmark_file_load_from_file   g_bookmark_file_load_from_file
#	define libslab_bookmark_file_to_file          g_bookmark_file_to_file

#	define libslab_bookmark_file_has_item         g_bookmark_file_has_item
#	define libslab_bookmark_file_remove_item      g_bookmark_file_remove_item
#	define libslab_bookmark_file_get_uris         g_bookmark_file_get_uris
#	define libslab_bookmark_file_get_title        g_bookmark_file_get_title
#	define libslab_bookmark_file_set_title        g_bookmark_file_set_title
#	define libslab_bookmark_file_get_mime_type    g_bookmark_file_get_mime_type
#	define libslab_bookmark_file_set_mime_type    g_bookmark_file_set_mime_type
#	define libslab_bookmark_file_get_modified     g_bookmark_file_get_modified
#	define libslab_bookmark_file_set_modified     g_bookmark_file_set_modified
#	define libslab_bookmark_file_get_applications g_bookmark_file_get_applications
#	define libslab_bookmark_file_add_application  g_bookmark_file_add_application
#	define libslab_bookmark_file_get_app_info     g_bookmark_file_get_app_info
#else
#	include "eggbookmarkfile.h"

#	define LibSlabBookmarkFile                    EggBookmarkFile

#	define libslab_bookmark_file_new              egg_bookmark_file_new
#	define libslab_bookmark_file_free             egg_bookmark_file_free

#	define libslab_bookmark_file_load_from_file   egg_bookmark_file_load_from_file
#	define libslab_bookmark_file_to_file          egg_bookmark_file_to_file

#	define libslab_bookmark_file_has_item         egg_bookmark_file_has_item
#	define libslab_bookmark_file_remove_item      egg_bookmark_file_remove_item
#	define libslab_bookmark_file_get_uris         egg_bookmark_file_get_uris
#	define libslab_bookmark_file_get_title        egg_bookmark_file_get_title
#	define libslab_bookmark_file_set_title        egg_bookmark_file_set_title
#	define libslab_bookmark_file_get_mime_type    egg_bookmark_file_get_mime_type
#	define libslab_bookmark_file_set_mime_type    egg_bookmark_file_set_mime_type
#	define libslab_bookmark_file_get_modified     egg_bookmark_file_get_modified
#	define libslab_bookmark_file_set_modified     egg_bookmark_file_set_modified
#	define libslab_bookmark_file_add_application  egg_bookmark_file_add_application
#	define libslab_bookmark_file_get_applications egg_bookmark_file_get_applications
#	define libslab_bookmark_file_get_app_info     egg_bookmark_file_get_app_info
#endif

G_BEGIN_DECLS

gboolean          libslab_gtk_image_set_by_id (GtkImage *image, const gchar *id);
GnomeDesktopItem *libslab_gnome_desktop_item_new_from_unknown_id (const gchar *id);
gboolean          libslab_gnome_desktop_item_launch_default (GnomeDesktopItem *item);
gchar            *libslab_gnome_desktop_item_get_docpath (GnomeDesktopItem *item);
gboolean          libslab_gnome_desktop_item_open_help (GnomeDesktopItem *item);
guint32           libslab_get_current_time_millis (void);
gint              libslab_strcmp (const gchar *a, const gchar *b);
gint              libslab_strlen (const gchar *a);
gpointer          libslab_get_gconf_value (const gchar *key);
void              libslab_set_gconf_value (const gchar *key, gconstpointer data);
void              libslab_handle_g_error (GError **error, const gchar *msg_format, ...);

GList *libslab_get_system_item_uris (void);
GList *libslab_get_user_app_uris    (void);
GList *libslab_get_user_doc_uris    (void);

gchar *libslab_get_system_item_store_path (gboolean writeable);
gchar *libslab_get_user_apps_store_path   (gboolean writeable);
gchar *libslab_get_user_docs_store_path   (gboolean writeable);

gboolean libslab_system_item_store_has_uri (const gchar *uri);
gboolean libslab_user_apps_store_has_uri   (const gchar *uri);
gboolean libslab_user_docs_store_has_uri   (const gchar *uri);

void libslab_remove_system_item (const gchar *uri);

void libslab_save_system_item_uris (const GList *);
void libslab_save_app_uris         (const GList *);
void libslab_save_doc_uris         (const GList *);

GnomeVFSMonitorHandle *libslab_add_system_item_monitor (GnomeVFSMonitorCallback callback,
                                                        gpointer                user_data);
GnomeVFSMonitorHandle *libslab_add_apps_monitor        (GnomeVFSMonitorCallback callback,
                                                        gpointer                user_data);
GnomeVFSMonitorHandle *libslab_add_docs_monitor        (GnomeVFSMonitorCallback callback,
                                                        gpointer                user_data);

typedef void (* libslab_marshal_func_VOID__POINTER_POINTER) (gpointer, gpointer, gpointer, gpointer);
void libslab_cclosure_marshal_VOID__POINTER_POINTER (GClosure     *,
                                                     GValue       *,
                                                     guint         ,
                                                     const GValue *,
                                                     gpointer      ,
                                                     gpointer);

G_END_DECLS

#endif
