#ifndef __MAIN_MENU_UTILS_H__
#define __MAIN_MENU_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

GList *get_system_item_uris     (void);
void   save_system_item_uris    (const GList *);
gchar *get_system_bookmark_path (void);

G_END_DECLS

#endif
