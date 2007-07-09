#ifndef __STORAGE_TILE_H__
#define __STORAGE_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define STORAGE_TILE_TYPE         (storage_tile_get_type ())
#define STORAGE_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), STORAGE_TILE_TYPE, StorageTile))
#define STORAGE_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), STORAGE_TILE_TYPE, StorageTileClass))
#define IS_STORAGE_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), STORAGE_TILE_TYPE))
#define IS_STORAGE_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), STORAGE_TILE_TYPE))
#define STORAGE_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), STORAGE_TILE_TYPE, StorageTileClass))

typedef struct {
	Tile tile;
} StorageTile;

typedef struct {
	TileClass tile_class;
} StorageTileClass;

GType storage_tile_get_type (void);

StorageTile *storage_tile_new (void);

G_END_DECLS

#endif
