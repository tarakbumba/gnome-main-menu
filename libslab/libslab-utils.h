#ifndef __GNOME_UTILS_H__
#define __GNOME_UTILS_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-desktop-item.h>
#include <libgnomevfs/gnome-vfs.h>

G_BEGIN_DECLS

gboolean          libslab_gtk_image_set_by_id (GtkImage *image, const gchar *id);
GnomeDesktopItem *libslab_gnome_desktop_item_new_from_unknown_id (const gchar *id);
gboolean          libslab_gnome_desktop_item_launch_default (GnomeDesktopItem *item);
gchar            *libslab_gnome_desktop_item_get_docpath (GnomeDesktopItem *item);
gboolean          libslab_gnome_desktop_item_open_help (GnomeDesktopItem *item);
guint32           libslab_get_current_time_millis (void);
gint              libslab_strcmp (const gchar *a, const gchar *b);
gpointer          libslab_get_gconf_value (const gchar *key);
void              libslab_set_gconf_value (const gchar *key, gconstpointer data);
void              libslab_handle_g_error (GError **error, const gchar *msg_format, ...);

GList *libslab_get_system_item_uris (void);
GList *libslab_get_app_uris         (void);

void libslab_save_system_item_uris (const GList *);
void libslab_save_app_uris         (const GList *);

GnomeVFSMonitorHandle *libslab_add_system_item_monitor (GnomeVFSMonitorCallback callback,
                                                        gpointer                user_data);
GnomeVFSMonitorHandle *libslab_add_apps_monitor        (GnomeVFSMonitorCallback callback,
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
