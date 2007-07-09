#ifndef __STORAGE_TILE_MODEL_H__
#define __STORAGE_TILE_MODEL_H__

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define STORAGE_TILE_MODEL_TYPE         (storage_tile_model_get_type ())
#define STORAGE_TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), STORAGE_TILE_MODEL_TYPE, StorageTileModel))
#define STORAGE_TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), STORAGE_TILE_MODEL_TYPE, StorageTileModelClass))
#define IS_STORAGE_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), STORAGE_TILE_MODEL_TYPE))
#define IS_STORAGE_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), STORAGE_TILE_MODEL_TYPE))
#define STORAGE_TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), STORAGE_TILE_MODEL_TYPE, StorageTileModelClass))

typedef struct {
	TileModel tile_model;
} StorageTileModel;

typedef struct {
	TileModelClass tile_model_class;
} StorageTileModelClass;

typedef struct {
	guint64 available_bytes;
	guint64 capacity_bytes;
} StorageTileModelStorageInfo;

GType storage_tile_model_get_type (void);

StorageTileModel *storage_tile_model_new (void);

TileAttribute *storage_tile_model_get_info_attr (StorageTileModel *this);

void           storage_tile_model_open          (StorageTileModel *this);
void           storage_tile_model_start_poll    (StorageTileModel *this);
void           storage_tile_model_stop_poll     (StorageTileModel *this);

G_END_DECLS

#endif
