#ifndef __DIRECTORY_TILE_H__
#define __DIRECTORY_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define DIRECTORY_TILE_TYPE         (directory_tile_get_type ())
#define DIRECTORY_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DIRECTORY_TILE_TYPE, DirectoryTile))
#define DIRECTORY_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DIRECTORY_TILE_TYPE, DirectoryTileClass))
#define IS_DIRECTORY_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DIRECTORY_TILE_TYPE))
#define IS_DIRECTORY_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DIRECTORY_TILE_TYPE))
#define DIRECTORY_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DIRECTORY_TILE_TYPE, DirectoryTileClass))

typedef struct {
	Tile tile;
} DirectoryTile;

typedef struct {
	TileClass tile_class;
} DirectoryTileClass;

GType directory_tile_get_type (void);

DirectoryTile *directory_tile_new (const gchar *uri);

G_END_DECLS

#endif
