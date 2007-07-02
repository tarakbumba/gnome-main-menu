#ifndef __CONTEXT_MENU_VIEW_H__
#define __CONTEXT_MENU_VIEW_H__

#include <gtk/gtk.h>

#include "tile-attribute.h"

G_BEGIN_DECLS

#define CONTEXT_MENU_VIEW_TYPE         (context_menu_view_get_type ())
#define CONTEXT_MENU_VIEW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CONTEXT_MENU_VIEW_TYPE, ContextMenuView))
#define CONTEXT_MENU_VIEW_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), CONTEXT_MENU_VIEW_TYPE, ContextMenuViewClass))
#define IS_CONTEXT_MENU_VIEW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CONTEXT_MENU_VIEW_TYPE))
#define IS_CONTEXT_MENU_VIEW_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), CONTEXT_MENU_VIEW_TYPE))
#define CONTEXT_MENU_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CONTEXT_MENU_VIEW_TYPE, ContextMenuViewClass))

#define CONTEXT_MENU_VIEW_MENU_ITEM_ATTR_PREFIX "context-menu-item-"
#define CONTEXT_MENU_VIEW_MENU_ITEM_ID_KEY      "context-menu-item-id"

typedef struct {
	GtkMenu gtk_menu;
} ContextMenuView;

typedef struct {
	GtkMenuClass gtk_menu_class;
} ContextMenuViewClass;

GType context_menu_view_get_type (void);

ContextMenuView *context_menu_view_new           (void);
TileAttribute   *context_menu_view_add_menu_item (ContextMenuView *this, GtkWidget *menu_item);

G_END_DECLS

#endif
