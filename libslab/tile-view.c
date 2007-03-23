#include "tile-view.h"

GType
tile_view_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (TileViewInterface),
			NULL, NULL,
			NULL, NULL, NULL,
			0, 0, NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE, "TileView", & info, 0);
	}

	return type;
}

TileAttribute *
tile_view_get_attribute_by_id (TileView *this, const gchar *uri)
{
	TileViewInterface *iface = TILE_VIEW_GET_INTERFACE (this);

	if (iface->get_attribute_by_id)
		return iface->get_attribute_by_id (this, uri);

	return NULL;
}
