#ifndef __TILE_H__
#define __TILE_H__

#include <gtk/gtk.h>

#include "tile-model.h"

G_BEGIN_DECLS

#define TILE_TYPE         (tile_get_type ())
#define TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_TYPE, Tile))
#define TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_TYPE, TileClass))
#define IS_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_TYPE))
#define IS_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_TYPE))
#define TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_TYPE, TileClass))

#define TILE_DEBOUNCE_PROP     "tile-debounce"
#define TILE_URI_PROP          "tile-uri"
#define TILE_CONTEXT_MENU_PROP "tile-context-menu"

#define TILE_ACTION_TRIGGERED_SIGNAL "tile-action-triggered"

enum {
	TILE_ACTION_LAUNCHES_APP = 1 << 0
};

typedef struct {
	GtkButton gtk_button;
} Tile;

typedef struct {
	GtkButtonClass gtk_button_class;

	void (* primary_action)   (Tile *tile);
	void (* action_triggered) (Tile *tile, guint action_flags);
} TileClass;

GType tile_get_type (void);

const gchar *tile_get_uri (Tile *this);

void     tile_action_triggered (Tile *this, guint action_flags);
gboolean tile_equals           (gconstpointer a, gconstpointer b);
gint     tile_compare          (gconstpointer a, gconstpointer b);

G_END_DECLS

#endif
