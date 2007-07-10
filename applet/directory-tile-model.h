#ifndef __DIRECTORY_TILE_MODEL_H__
#define __DIRECTORY_TILE_MODEL_H__

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define DIRECTORY_TILE_MODEL_TYPE         (directory_tile_model_get_type ())
#define DIRECTORY_TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DIRECTORY_TILE_MODEL_TYPE, DirectoryTileModel))
#define DIRECTORY_TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DIRECTORY_TILE_MODEL_TYPE, DirectoryTileModelClass))
#define IS_DIRECTORY_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DIRECTORY_TILE_MODEL_TYPE))
#define IS_DIRECTORY_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DIRECTORY_TILE_MODEL_TYPE))
#define DIRECTORY_TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DIRECTORY_TILE_MODEL_TYPE, DirectoryTileModelClass))

typedef struct {
	TileModel tile_model;
} DirectoryTileModel;

typedef struct {
	TileModelClass tile_model_class;
} DirectoryTileModelClass;

GType directory_tile_model_get_type (void);

DirectoryTileModel *directory_tile_model_new (const gchar *uri);

TileAttribute *directory_tile_model_get_icon_id_attr    (DirectoryTileModel *this);
TileAttribute *directory_tile_model_get_name_attr       (DirectoryTileModel *this);
TileAttribute *directory_tile_model_get_is_local_attr   (DirectoryTileModel *this);
TileAttribute *directory_tile_model_get_can_delete_attr (DirectoryTileModel *this);

void           directory_tile_model_open                (DirectoryTileModel *this);
void           directory_tile_model_send_to             (DirectoryTileModel *this);
void           directory_tile_model_trash               (DirectoryTileModel *this);
void           directory_tile_model_delete              (DirectoryTileModel *this);

G_END_DECLS

#endif
