#ifndef __TILE_H__
#define __TILE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TILE_TYPE         (tile_get_type ())
#define TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_TYPE, Tile))
#define TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_TYPE, TileClass))
#define IS_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_TYPE))
#define IS_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_TYPE))
#define TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_TYPE, TileClass))

#define TILE_ACTION_TRIGGERED_SIGNAL "tile-action-triggered"

enum {
	TILE_ACTION_LAUNCHES_APP = 1 << 0
};

typedef struct {
	GObject g_object;
} Tile;

typedef struct {
	GObjectClass g_object_class;

	GtkWidget * (* get_widget) (Tile *this);
	gboolean    (* equals)     (Tile *this, gconstpointer);
} TileClass;

GType tile_get_type (void);

GtkWidget *tile_get_widget           (Tile *this);
Tile      *tile_get_tile_from_widget (GtkWidget *widget);
gboolean   tile_equals               (Tile *this, gconstpointer that);
void       tile_action_triggered     (Tile *this, guint action_flags);

G_END_DECLS

#endif
