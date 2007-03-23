#ifndef __TILE_MODEL_H__
#define __TILE_MODEL_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define TILE_MODEL_TYPE         (tile_model_get_type ())
#define TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TILE_MODEL_TYPE, TileModel))
#define TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TILE_MODEL_TYPE, TileModelClass))
#define IS_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TILE_MODEL_TYPE))
#define IS_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), TILE_MODEL_TYPE))
#define TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TILE_MODEL_TYPE, TileModelClass))

#define TILE_MODEL_URI_PROP "tile-model-uri"

typedef struct {
	GObject g_object;
} TileModel;

typedef struct {
	GObjectClass g_object_class;
} TileModelClass;

GType tile_model_get_type (void);

const gchar *tile_model_get_uri (TileModel *this);
void         tile_model_set_uri (TileModel *this, const gchar *uri);

G_END_DECLS

#endif
