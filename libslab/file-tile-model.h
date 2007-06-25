#ifndef __FILE_TILE_MODEL_H__
#define __FILE_TILE_MODEL_H__

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define FILE_TILE_MODEL_TYPE         (file_tile_model_get_type ())
#define FILE_TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), FILE_TILE_MODEL_TYPE, FileTileModel))
#define FILE_TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), FILE_TILE_MODEL_TYPE, FileTileModelClass))
#define IS_FILE_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), FILE_TILE_MODEL_TYPE))
#define IS_FILE_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), FILE_TILE_MODEL_TYPE))
#define FILE_TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), FILE_TILE_MODEL_TYPE, FileTileModelClass))

typedef struct {
	TileModel tile_model;
} FileTileModel;

typedef struct {
	TileModelClass tile_model_class;
} FileTileModelClass;

GType file_tile_model_get_type (void);

FileTileModel *file_tile_model_new (const gchar *uri);

TileAttribute *file_tile_model_get_file_name_attr   (FileTileModel *this);
TileAttribute *file_tile_model_get_icon_id_attr     (FileTileModel *this);
TileAttribute *file_tile_model_get_mtime_attr       (FileTileModel *this);
TileAttribute *file_tile_model_get_app_attr         (FileTileModel *this);
void           file_tile_model_open                 (FileTileModel *this);
void           file_tile_model_open_in_file_browser (FileTileModel *this);
void           file_tile_model_rename               (FileTileModel *this, const gchar *name);

G_END_DECLS

#endif
