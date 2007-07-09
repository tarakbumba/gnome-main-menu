#ifndef __NETWORK_TILE_H__
#define __NETWORK_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define NETWORK_TILE_TYPE         (network_tile_get_type ())
#define NETWORK_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETWORK_TILE_TYPE, NetworkTile))
#define NETWORK_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), NETWORK_TILE_TYPE, NetworkTileClass))
#define IS_NETWORK_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETWORK_TILE_TYPE))
#define IS_NETWORK_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), NETWORK_TILE_TYPE))
#define NETWORK_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NETWORK_TILE_TYPE, NetworkTileClass))

typedef struct {
	Tile tile;
} NetworkTile;

typedef struct {
	TileClass tile_class;
} NetworkTileClass;

GType network_tile_get_type (void);

NetworkTile *network_tile_new (void);

G_END_DECLS

#endif
