#ifndef __TILE_CONTROL_H__
#define __TILE_CONTROL_H__

#include <glib.h>

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define TILE_CONTROL_TYPE         (tile_control_get_type ())
#define TILE_CONTROL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_CONTROL_TYPE, TileControl))
#define TILE_CONTROL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_CONTROL_TYPE, TileControlClass))
#define IS_TILE_CONTROL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_CONTROL_TYPE))
#define IS_TILE_CONTROL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_CONTROL_TYPE))
#define TILE_CONTROL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_CONTROL_TYPE, TileControlClass))

typedef void (* TileControlMappingFunc) (const GValue *, GValue *, gpointer);

typedef struct {
	GObject g_object;
} TileControl;

typedef struct {
	GObjectClass g_object_class;
} TileControlClass;

GType tile_control_get_type (void);

TileControl *tile_control_new                   (TileAttribute *source, TileAttribute *destination);
TileControl *tile_control_new_with_mapping_func (TileAttribute *source, TileAttribute *destination,
                                                 TileControlMappingFunc func, gpointer data);

G_END_DECLS

#endif
