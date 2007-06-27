#ifndef __APPLICATION_TILE_H__
#define __APPLICATION_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define APPLICATION_TILE_TYPE         (application_tile_get_type ())
#define APPLICATION_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), APPLICATION_TILE_TYPE, ApplicationTile))
#define APPLICATION_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), APPLICATION_TILE_TYPE, ApplicationTileClass))
#define IS_APPLICATION_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), APPLICATION_TILE_TYPE))
#define IS_APPLICATION_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), APPLICATION_TILE_TYPE))
#define APPLICATION_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), APPLICATION_TILE_TYPE, ApplicationTileClass))

typedef struct {
	Tile tile;
} ApplicationTile;

typedef struct {
	TileClass tile_class;
} ApplicationTileClass;

GType application_tile_get_type (void);

ApplicationTile *application_tile_new (const gchar *desktop_item_id);

G_END_DECLS

#endif
