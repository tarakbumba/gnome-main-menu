#ifndef __MAIN_MENU_UTILS_H__
#define __MAIN_MENU_UTILS_H__

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>

G_BEGIN_DECLS

GList *get_system_item_uris (void);
GList *get_app_uris         (void);

void save_system_item_uris (const GList *);
void save_app_uris         (const GList *);

GnomeVFSMonitorHandle *add_system_item_monitor (GnomeVFSMonitorCallback callback,
                                                gpointer                user_data);
GnomeVFSMonitorHandle *add_apps_monitor        (GnomeVFSMonitorCallback callback,
                                                gpointer                user_data);

G_END_DECLS

#endif
