#ifndef __DOCUMENT_TILE_H__
#define __DOCUMENT_TILE_H__

#include "tile.h"

G_BEGIN_DECLS

#define DOCUMENT_TILE_TYPE         (document_tile_get_type ())
#define DOCUMENT_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DOCUMENT_TILE_TYPE, DocumentTile))
#define DOCUMENT_TILE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DOCUMENT_TILE_TYPE, DocumentTileClass))
#define IS_DOCUMENT_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DOCUMENT_TILE_TYPE))
#define IS_DOCUMENT_TILE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DOCUMENT_TILE_TYPE))
#define DOCUMENT_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DOCUMENT_TILE_TYPE, DocumentTileClass))

typedef struct {
	Tile tile;
} DocumentTile;

typedef struct {
	TileClass tile_class;
} DocumentTileClass;

GType document_tile_get_type (void);

GtkWidget *document_tile_new (const gchar *uri);

G_END_DECLS

#endif
