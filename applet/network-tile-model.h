#ifndef __NETWORK_TILE_MODEL_H__
#define __NETWORK_TILE_MODEL_H__

#include "tile-model.h"
#include "tile-attribute.h"

G_BEGIN_DECLS

#define NETWORK_TILE_MODEL_TYPE         (network_tile_model_get_type ())
#define NETWORK_TILE_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NETWORK_TILE_MODEL_TYPE, NetworkTileModel))
#define NETWORK_TILE_MODEL_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), NETWORK_TILE_MODEL_TYPE, NetworkTileModelClass))
#define IS_NETWORK_TILE_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NETWORK_TILE_MODEL_TYPE))
#define IS_NETWORK_TILE_MODEL_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), NETWORK_TILE_MODEL_TYPE))
#define NETWORK_TILE_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NETWORK_TILE_MODEL_TYPE, NetworkTileModelClass))

typedef struct {
	TileModel tile_model;
} NetworkTileModel;

typedef struct {
	TileModelClass tile_model_class;
} NetworkTileModelClass;

typedef enum {
	NETWORK_TILE_MODEL_TYPE_NONE,
	NETWORK_TILE_MODEL_TYPE_WIRED,
	NETWORK_TILE_MODEL_TYPE_WIRELESS
} NetworkTileModelNetworkType;

typedef struct {
	NetworkTileModelNetworkType  type;
	gchar                       *essid;
	gchar                       *iface;
} NetworkTileModelNetworkInfo;

GType network_tile_model_get_type (void);

NetworkTileModel *network_tile_model_new (void);

TileAttribute *network_tile_model_get_info_attr (NetworkTileModel *this);

void           network_tile_model_open_info     (NetworkTileModel *this);

G_END_DECLS

#endif
