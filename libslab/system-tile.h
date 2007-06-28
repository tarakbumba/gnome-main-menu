#ifndef __SYSTEM_TILE_H__
#define __SYSTEM_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define SYSTEM_TILE_TYPE         (system_tile_get_type ())
#define SYSTEM_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SYSTEM_TILE_TYPE, SystemTile))
#define SYSTEM_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), SYSTEM_TILE_TYPE, SystemTileClass))
#define IS_SYSTEM_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SYSTEM_TILE_TYPE))
#define IS_SYSTEM_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), SYSTEM_TILE_TYPE))
#define SYSTEM_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SYSTEM_TILE_TYPE, SystemTileClass))

typedef struct {
	Tile tile;
} SystemTile;

typedef struct {
	TileClass tile_class;
} SystemTileClass;

GType system_tile_get_type (void);

SystemTile *system_tile_new (const gchar *desktop_item_id, const gchar *title);

G_END_DECLS

#endif
