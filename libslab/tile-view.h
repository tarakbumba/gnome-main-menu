#ifndef __TILE_VIEW_H__
#define __TILE_VIEW_H__

#include <glib-object.h>

#include "tile-attribute.h"

G_BEGIN_DECLS

#define TILE_VIEW_TYPE             (tile_view_get_type ())
#define TILE_VIEW(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_VIEW_TYPE, TileView))
#define IS_TILE_VIEW(o)            (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_VIEW_TYPE))
#define TILE_VIEW_GET_INTERFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), TILE_VIEW_TYPE, TileViewInterface))

typedef struct _TileView TileView;

typedef struct {
	GTypeInterface g_type_interface;

	TileAttribute * (* get_attribute_by_id) (TileView *this, const gchar *id);
} TileViewInterface;

GType tile_view_get_type (void);

TileAttribute *tile_view_get_attribute_by_id (TileView *this, const gchar *id);

G_END_DECLS

#endif
