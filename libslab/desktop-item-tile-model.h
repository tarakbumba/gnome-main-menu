#ifndef __DESKTOP_ITEM_TILE_MODEL_H__
#define __DESKTOP_ITEM_TILE_MODEL_H__

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define DESKTOP_ITEM_TILE_MODEL_TYPE         (desktop_item_tile_model_get_type ())
#define DESKTOP_ITEM_TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), DESKTOP_ITEM_TILE_MODEL_TYPE, DesktopItemTileModel))
#define DESKTOP_ITEM_TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), DESKTOP_ITEM_TILE_MODEL_TYPE, DesktopItemTileModelClass))
#define IS_DESKTOP_ITEM_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), DESKTOP_ITEM_TILE_MODEL_TYPE))
#define IS_DESKTOP_ITEM_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), DESKTOP_ITEM_TILE_MODEL_TYPE))
#define DESKTOP_ITEM_TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DESKTOP_ITEM_TILE_MODEL_TYPE, DesktopItemTileModelClass))

#define DESKTOP_ITEM_TILE_MODEL_STORE_TYPE_PROP "store-type"

typedef struct {
	TileModel tile_model;
} DesktopItemTileModel;

typedef struct {
	TileModelClass tile_model_class;
} DesktopItemTileModelClass;

typedef enum {
	APP_NOT_IN_AUTOSTART_DIR,
	APP_IN_USER_AUTOSTART_DIR,
	APP_IN_SYSTEM_AUTOSTART_DIR,
	APP_NOT_ELIGIBLE
} DesktopItemAutostartStatus;

GType desktop_item_tile_model_get_type (void);

DesktopItemTileModel *desktop_item_tile_model_new (const gchar *desktop_item_id);

TileAttribute *desktop_item_tile_model_get_icon_id_attr            (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_name_attr               (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_description_attr        (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_comment_attr            (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_help_available_attr     (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_is_in_store_attr        (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_store_status_attr       (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_autostart_status_attr   (DesktopItemTileModel *this);
TileAttribute *desktop_item_tile_model_get_can_manage_package_attr (DesktopItemTileModel *this);

void           desktop_item_tile_model_start                       (DesktopItemTileModel *this);
void           desktop_item_tile_model_open_help                   (DesktopItemTileModel *this);
void           desktop_item_tile_model_user_store_toggle           (DesktopItemTileModel *this);
void           desktop_item_tile_model_autostart_toggle            (DesktopItemTileModel *this);
void           desktop_item_tile_model_upgrade_package             (DesktopItemTileModel *this);
void           desktop_item_tile_model_uninstall_package           (DesktopItemTileModel *this);

G_END_DECLS

#endif
